# Architecture

## Source layout

```
elevation_dll/
  include/
    elevation_api.h        ← public C ABI
  src/
    elevation_service.cpp  ← implements the C API; owns TileStore
    tile_store.h / .cpp    ← fixed-capacity LRU of mmap'd tiles, thread-safe
    bicubic.h / .cpp       ← Catmull-Rom 4×4 kernel (self-contained copy)
    dt2_read.h             ← inline helpers: path builder, byte reader, offsets
    mapped_file.h / .cpp   ← thin Win32 / POSIX mmap wrapper
```

---

## Component responsibilities

### `dt2_read` (header-only)

Provides DTED2 constants, the post-offset formula, and byte helpers.

```c
static const long DTED_HEADER     = 3428;
static const long DTED_REC_HEADER = 8;
static const long DTED_REC_FOOTER = 4;
static const int  DTED_DIM        = 3601;
static const long DTED_COL_STRIDE = 7214;   /* 8 + 3601*2 + 4 */
static const long DTED_FILE_SIZE  = 25981042;
static const int  DTED_VOID       = -32768;

/* Byte offset of post (col c, row r) inside the mmap'd file */
static inline long dted_post_offset(int c, int r) {
    return DTED_HEADER + (long)c * DTED_COL_STRIDE + DTED_REC_HEADER + (long)r * 2;
}

/* Read big-endian int16 from two raw bytes */
static inline short read_be_i16(const unsigned char* p) {
    return (short)((unsigned short)((p[0] << 8) | p[1]));
}

/* Decode post value — handles signed-magnitude encoding used by DTED2 */
static inline int decode_dted_post(short raw) {
    if (raw == (short)DTED_VOID) return DTED_VOID;
    if (raw < 0)                 return -(raw & 0x7FFF);  /* signed-magnitude */
    return (int)raw;
}
```

`build_tile_path()` produces e.g. `"<tiles_dir>/e034/n32.dt2"` using `snprintf`
into the caller's buffer — no heap allocation.

### `mapped_file`

Thin cross-platform wrapper hiding OS mmap differences:

```cpp
class MappedFile {
public:
    bool open(const char* path, long expected_size);
    void close();
    const unsigned char* data() const;
    bool is_missing() const;   // true iff last open() failed with "file not found"
};
```

- **Win32**: `CreateFileA` → `CreateFileMappingA` → `MapViewOfFile`.
  Missing detected via `ERROR_FILE_NOT_FOUND` / `ERROR_PATH_NOT_FOUND`.
- **POSIX**: `open(O_RDONLY)` → `fstat` → `mmap(PROT_READ, MAP_PRIVATE)`.
  Missing detected via `errno == ENOENT`.

Rejects files whose size ≠ `DTED_FILE_SIZE`. No heap allocation.

### `TileStore`

256-slot LRU cache of memory-mapped tiles. All storage is pre-allocated in the
`TileStore` object itself — no heap after construction.

```cpp
struct Slot {
    unsigned int key;    // (lat_floor+90) << 16 | (lon_floor+180); 0xFFFFFFFFu = empty
    MappedFile   mapped;
    int          prev, next;  // LRU doubly-linked list (indices into slots_[])
};
static const int CAPACITY      = 256;
static const int TILES_DIR_MAX = 512;
Slot slots_[CAPACITY];
int  lru_head_, lru_tail_;
char tiles_dir_[TILES_DIR_MAX];
std::mutex mutex_;
```

`get(lat_floor, lon_floor, &out_missing, &out_io_err)`:
1. Lock `mutex_`.
2. Linear scan `slots_` for the packed key (256 entries — L1 cache-friendly).
3. Hit: move slot to LRU head; return `data()`.
4. Miss: evict LRU tail, call `mapped.open()`, insert at head.
   - Missing file → negative-cache the slot (`is_missing()` stays true).
   - Wrong size / mmap fail → leave slot empty, set `*out_io_err`.

### `bicubic`

Pure function — Catmull-Rom kernel over a 4×4 grid:

```cpp
double bicubic_interpolate(const double p[4][4], double tx, double ty);
```

### `ElevationService`

Holds one `TileStore`. `es_get_elev` is the hot path:

1. Validate: `out_msl != NULL`, lat ∈ [-90,90], lon ∈ [-180,180].
2. `lat_floor = floor(lat)`, `lon_floor = floor(lon)`.
3. Sub-tile fractional position:
   - `row_f = (lat - lat_floor) * (DTED_DIM - 1)` — 0 = south edge, 3600 = north
   - `col_f = (lon - lon_floor) * (DTED_DIM - 1)` — 0 = west edge,  3600 = east
4. `r = floor(row_f)`, `c = floor(col_f)`, `ty = row_f - r`, `tx = col_f - c`.
5. Get tile pointer via `TileStore::get`.
6. Fill `double p[4][4]` from neighborhood `[c-1..c+2] × [r-1..r+2]`,
   clamped to `[0, DTED_DIM-1]`. Each post: `decode_dted_post(read_be_i16(...))`.
   If any post is `DTED_VOID` → return `ES_ERR_NO_DATA`.
7. `bicubic_interpolate(p, tx, ty)` → write `*out_msl` → return `ES_OK`.

Working memory: one `double p[4][4]` on the stack. Zero heap.

---

## DTED2 data format

### File-system layout (NGA convention)

```
<tiles_dir>/
  e034/              ← 'e' or 'w' + 3-digit zero-padded |lon_floor|
    n32.dt2          ← 'n' or 's' + 2-digit zero-padded |lat_floor|
    n33.dt2
  w074/
    n40.dt2
```

### File structure

| Region | Size (bytes) |
|--------|-------------|
| Header (UHL + DSI + ACC) | **3428** |
| 3601 columns, each: | |
| &nbsp;&nbsp;record header | **8** |
| &nbsp;&nbsp;3601 × int16 | 7202 |
| &nbsp;&nbsp;record footer (checksum) | **4** |

Total: `3428 + 3601 × 7214` = **25,981,042 bytes**.

### Encoding

- Storage order: **column-major** (west → east). Within each column: **south → north**
  (row 0 = south edge, row 3600 = north edge).
- Each post: **big-endian int16**.
- **Negative elevations** use **signed-magnitude** encoding, not two's-complement.
  Raw value `0x81 0x9F` = −415 m (Dead Sea), not −32353 m.
  `decode_dted_post()` handles this: `if (raw < 0) return -(raw & 0x7FFF)`.
- Void post: **−32768** (`DTED_VOID`) → `ES_ERR_NO_DATA`.

---

## Thread safety

- `es_get_elev` may be called concurrently from any number of threads.
- `TileStore` holds a single `TsMutex`. Every `get()` call takes a scoped lock
  via `TsLock`. The critical section is a linear scan of 256 packed 4-byte keys
  — typically under 100 ns — so contention is not measurable at mouse-hover or
  even high-rate radar target rates.
- `es_create` / `es_destroy` are **not** thread-safe versus concurrent queries.
  Caller must ensure no queries are in-flight during create/destroy.

### `TsMutex` / `TsLock` — portable mutex

Selected at compile time via `__cplusplus`:

| Compiler | Primitive used |
|----------|---------------|
| C++11 and later | `std::mutex` |
| C++98 / C++03 on Windows (Win98+) | `CRITICAL_SECTION` |
| C++98 / C++03 on POSIX | `pthread_mutex_t` |

The `TsLock` RAII guard (constructor locks, destructor unlocks) is written in
C++98-compatible style — no move semantics, copy suppressed via private
declaration.

---

## Memory profile

| Item | Value |
|------|-------|
| Virtual address per tile | ~26 MB (3601×3601×2 bytes mapped) |
| Max virtual (256 slots) | ~6.6 GB (fine on x64 / 32-bit processes need fewer slots) |
| Resident pages | Only pages actually read; typically a few MB for point queries |
| Heap after `es_create` | Zero |
