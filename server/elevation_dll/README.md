# elevation_dll — Global DTED2 Elevation Lookup

A standalone shared library (Windows `.dll` / Linux `.so`) exposing a C ABI for
**global terrain elevation (MSL) queries** from on-disk DTED Level 2 (`.dt2`)
tiles. Designed for high-rate point queries (e.g. mouse-hover on a map UI) with
**zero heap allocation on the query path** and a **bounded resident memory
footprint** regardless of how much terrain data exists on disk.

This project is **fully self-contained**. It does **not** link against, modify,
or share code with any other project in the solution. The existing radar logic
(`DemDatabase`, `DtedTile`, `QueryHandler`, servers) is **not touched**.

---

## Quick Start

### Step 1 — Build the library

**Windows (Visual Studio 2022)**

Open `server/radar_sea_level.sln`, set configuration to **Release | x64**, and
build the `elevation_dll` project. Outputs:

```
server/elevation_dll/x64/Release/elevation_dll.dll   ← runtime
server/elevation_dll/x64/Release/elevation_dll.lib   ← link-time import lib
```

**Linux**

```bash
cd server/elevation_dll
make           # produces libelevation.so
```

### Step 2 — Arrange your DTED2 tiles

Tiles must follow the NGA directory layout:

```
<tiles_dir>/
  e034/          ← lowercase e/w + 3-digit zero-padded longitude floor
    n32.dt2      ← lowercase n/s + 2-digit zero-padded latitude floor
    n33.dt2
  e035/
    n32.dt2
  ...
```

> **Note**: Windows NTFS is case-insensitive, so uppercase subdirectory names
> (`E034/N32.dt2`) also work on Windows. On Linux the names must be lowercase.

The path you pass to `es_create()` is the root directory that contains these
`e???/` subdirectories.

### Step 3 — Link and call

**C / C++**

```c
#include "elevation_api.h"         // copy from server/elevation_dll/include/

// Windows: link against elevation_dll.lib; put elevation_dll.dll next to the exe
// Linux:   compile with -L<dir> -lelevation

ElevationService* svc = es_create("/data/dted_maps/");
if (!svc) { /* handle error */ }

double msl;
int rc = es_get_elev(svc, 32.0853, 34.7818, &msl);   // Tel Aviv
if (rc == ES_OK) printf("%.1f m\n", msl);             // → ~20 m

es_destroy(svc);
```

**Python (ctypes)**

```python
import ctypes, sys

lib = ctypes.CDLL("elevation_dll.dll" if sys.platform == "win32"
                  else "./libelevation.so")

lib.es_create.restype  = ctypes.c_void_p
lib.es_create.argtypes = [ctypes.c_char_p]
lib.es_get_elev.restype  = ctypes.c_int
lib.es_get_elev.argtypes = [ctypes.c_void_p, ctypes.c_double,
                             ctypes.c_double, ctypes.POINTER(ctypes.c_double)]
lib.es_destroy.restype  = None
lib.es_destroy.argtypes = [ctypes.c_void_p]

svc = lib.es_create(b"/data/dted_maps/")
msl = ctypes.c_double()
rc  = lib.es_get_elev(svc, 32.0853, 34.7818, ctypes.byref(msl))
if rc == 0:
    print(f"{msl.value:.1f} m")
lib.es_destroy(svc)
```

**C# (P/Invoke)**

```csharp
using System.Runtime.InteropServices;

static class Elev {
    [DllImport("elevation_dll.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr es_create([MarshalAs(UnmanagedType.LPStr)] string tilesDir);

    [DllImport("elevation_dll.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern int es_get_elev(IntPtr svc, double lat, double lon, out double outMsl);

    [DllImport("elevation_dll.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern void es_destroy(IntPtr svc);
}

var svc = Elev.es_create(@"C:\data\dted_maps\");
int rc  = Elev.es_get_elev(svc, 32.0853, 34.7818, out double msl);
if (rc == 0) Console.WriteLine($"{msl:F1} m");
Elev.es_destroy(svc);
```

### Error codes

| Code | Value | Meaning |
|------|-------|---------|
| `ES_OK` | 0 | Success; `*out_msl` is valid |
| `ES_ERR_BAD_ARG` | -1 | Null pointer, or lat/lon out of `[-90,90]`/`[-180,180]` |
| `ES_ERR_NO_DATA` | -2 | No tile on disk for this coordinate, or DTED void post |
| `ES_ERR_IO` | -3 | File found but wrong size or mmap failed |

### Tiles path — runtime only, no config file

The tiles directory is passed to `es_create()` at runtime. There is no config
file or environment variable. To switch datasets, call `es_destroy()` followed
by `es_create()` with the new path.

---

## Consumer compatibility

The public surface is a **pure C ABI** (`extern "C"`, no C++ types, no STL).
Consumers may be written in:

- C (C89 and later)
- C++ (C++98, C++03, C++11, and later)
- Any language that can call a C-ABI shared library: C#, Python (ctypes/cffi),
  Rust, Go, Java (JNI), etc.

The public header `elevation_api.h` uses only `int`, `double`, `char`, and an
opaque `struct` forward declaration. No `<stdint.h>`, `<stdbool.h>`, `nullptr`,
or anything that would break C89/C++98 consumers.

---

## 1. Public API

The DLL exports a small C ABI. Header: `include/elevation_api.h`.

```c
#if defined(_WIN32)
  #if defined(ELEVATION_DLL_EXPORTS)
    #define ELEV_API __declspec(dllexport)
  #else
    #define ELEV_API __declspec(dllimport)
  #endif
  #define ELEV_CALL __cdecl
#else
  #if defined(ELEVATION_DLL_EXPORTS) && (defined(__GNUC__) || defined(__clang__))
    #define ELEV_API __attribute__((visibility("default")))
  #else
    #define ELEV_API
  #endif
  #define ELEV_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ElevationService ElevationService;

/* Error codes (returned by es_get_elev). 0 = success. */
#define ES_OK                 0
#define ES_ERR_BAD_ARG       -1   /* null pointer or out-of-range lat/lon */
#define ES_ERR_NO_DATA       -2   /* no tile on disk for this lat/lon, or void post */
#define ES_ERR_IO            -3   /* file open / mmap / unexpected file size */

/* Create a service rooted at tiles_dir (absolute or relative path).
   tiles_dir must contain DTED2 .dt2 files in NGA layout:
     <tiles_dir>/<lon_subdir>/<lat_file>.dt2
   where <lon_subdir> = "e034" or "w074" (lowercase, 3-digit zero-padded longitude)
   and   <lat_file>   = "n32"  or "s05"  (lowercase, 2-digit zero-padded latitude).
   Returns NULL on failure. Thread-safe to call once at startup. */
ELEV_API ElevationService* ELEV_CALL es_create(const char* tiles_dir);

/* Destroy the service. Unmaps all cached tiles. Not thread-safe vs. es_get_elev;
   the caller must ensure no queries are in flight. */
ELEV_API void ELEV_CALL es_destroy(ElevationService* svc);

/* Query terrain elevation at (lat_deg, lon_deg).
   On success: returns ES_OK and writes meters-MSL to *out_msl.
   On any error: returns negative code; *out_msl is left untouched.
   Thread-safe: may be called concurrently from many threads. */
ELEV_API int ELEV_CALL es_get_elev(ElevationService* svc,
                                   double lat_deg, double lon_deg,
                                   double* out_msl);

#ifdef __cplusplus
}
#endif
```

### Semantics

- **Coordinate range**: lat ∈ [-90, 90], lon ∈ [-180, 180]. Outside → `ES_ERR_BAD_ARG`.
- **No-data**: if the corresponding `.hgt` file does not exist on disk, return
  `ES_ERR_NO_DATA`. The caller decides how to interpret (likely sea → 0 m).
- **SRTM void value** `-32768` in the raw data is also treated as `ES_ERR_NO_DATA`.
- **Interpolation**: bicubic (Catmull-Rom) over the 4×4 neighborhood of posts.
- **Cross-tile interpolation is NOT required for v1.** If the 4×4 neighborhood
  would cross a tile boundary, clamp the neighborhood to the current tile.
  (Documented limitation. Acceptable for hover-on-map UX.)

---

## 2. Data Format Assumptions

Only **DTED Level 2** (`.dt2`) is supported. The on-disk layout matches the
existing `DtedTile::loadDTED2` in `server/src/dted_tile.cpp` (do not include
that file — match its conventions, copy the constants).

### File-system layout (NGA convention)

```
<tiles_dir>/
  e034/
    n32.dt2
    n33.dt2
  w074/
    n40.dt2
  ...
```

- Longitude subdirectory: `e` or `w` + 3-digit zero-padded `|lon_floor|`.
  Examples: lon_floor = 34 → `e034`, lon_floor = -74 → `w074`, lon_floor = 0 → `e000`.
- Latitude file: `n` or `s` + 2-digit zero-padded `|lat_floor|`, extension `.dt2`.
  Examples: lat_floor = 32 → `n32.dt2`, lat_floor = -5 → `s05.dt2`, lat_floor = 0 → `n00.dt2`.
- `lat_floor = floor(lat_deg)`, `lon_floor = floor(lon_deg)`.

### File structure

| Region                    | Size (bytes)            |
|---------------------------|-------------------------|
| Header (UHL + DSI + ACC)  | **3428**                |
| 3601 columns, each:       |                         |
| &nbsp;&nbsp;record header | **8**                   |
| &nbsp;&nbsp;3601 × int16  | 7202                    |
| &nbsp;&nbsp;record footer | **4** (checksum)        |

Total file size: `3428 + 3601 * (8 + 3601*2 + 4)` = `3428 + 3601 * 7214`
= **25,981,042 bytes**. Reject any file with a different size as `ES_ERR_IO`.

### Cell layout

- 3601 × 3601 posts per 1°×1° tile, 1 arc-second nominal post spacing.
- Storage order in the file: **column-major** (west → east). Each column stores
  posts **south → north** (row 0 = south edge of cell, row 3600 = north edge).
- Encoding: **big-endian int16**, signed two's-complement (match existing code —
  do not implement signed-magnitude even though DTED spec mentions it; the
  existing loader treats it as plain big-endian two's-complement).
- Void value: **-32768** (match existing `DtedTile::postElevation`).

### Byte offset of post (col c, row r) within the mapped file

```
static const long DTED_HEADER     = 3428;
static const long DTED_REC_HEADER = 8;
static const long DTED_REC_FOOTER = 4;
static const int  DTED_DIM        = 3601;            /* posts per side  */
static const long DTED_COL_STRIDE = DTED_REC_HEADER  /* 8               */
                                  + DTED_DIM * 2     /* 7202            */
                                  + DTED_REC_FOOTER; /* 4  → 7214 total */
static const long DTED_FILE_SIZE  = DTED_HEADER + (long)DTED_DIM * DTED_COL_STRIDE;
                                                     /* 25,981,042      */
static const int  DTED_VOID       = -32768;

/* Byte offset of post (c, r) inside the mmap'd file. */
static inline long dted_post_offset(int c, int r) {
    return DTED_HEADER
         + (long)c * DTED_COL_STRIDE
         + DTED_REC_HEADER
         + (long)r * 2;
}
```

### Cross-tile bicubic neighborhood

Same rule as v1 SRTM design: **do not cross tile boundaries**. Clamp the 4×4
neighborhood `[c-1..c+2] × [r-1..r+2]` to `[0, DTED_DIM-1]`. Documented limitation.

### Polar / non-uniform DTED2

DTED2 specifies coarser longitudinal spacing above |lat| ≥ 50° (column count
drops from 3601 to 1801 etc.). **v1 does not support this**: if `lat_floor` is
outside `[-50, 49]`, attempt the lookup anyway with the 3601 assumption, and
return `ES_ERR_IO` if the file size doesn't match. Document this as a known
limitation; the radar coverage area (Israel) is well inside ±50°.

---

## 3. Architecture

```
elevation_dll/
  include/
    elevation_api.h        ← public C ABI (above)
  src/
    elevation_service.cpp  ← implements the C API; owns TileStore
    tile_store.h / .cpp    ← fixed-capacity LRU of mmap'd tiles, thread-safe
    bicubic.h / .cpp       ← Catmull-Rom 4x4 kernel (COPIED, not shared)
    dt2_read.h             ← inline helpers: path builder, big-endian int16, offsets
    mapped_file.h / .cpp   ← thin Win32 / POSIX mmap wrapper (cross-platform)
  README.md                ← this file
```

### Component responsibilities

#### `bicubic` (copied math — no dependency on existing code)
- Pure function: `double bicubic_interpolate(const double p[4][4], double tx, double ty);`
- Catmull-Rom kernel. Re-derive or copy from `server/src/dem_database.cpp` — but
  **do not `#include` any header from outside `elevation_dll/`**.

#### `dt2_read` (header-only helpers)
- `bool build_tile_path(const char* tiles_dir, int lat_floor, int lon_floor, char* out, int out_cap);`
  Produces e.g. `"<tiles_dir>/e034/n32.dt2"`. Returns false if `out_cap`
  is insufficient. **Must not allocate** — use `snprintf` into the caller's buffer.
- `static inline short read_be_i16(const unsigned char* p)
   { return (short)((unsigned short)((p[0] << 8) | p[1])); }`
- DTED constants: `DTED_HEADER = 3428`, `DTED_REC_HEADER = 8`, `DTED_REC_FOOTER = 4`,
  `DTED_DIM = 3601`, `DTED_COL_STRIDE = 7214`, `DTED_FILE_SIZE = 25981042`,
  `DTED_VOID = -32768`.
- `static inline long dted_post_offset(int c, int r);` — see section 2.

#### `mapped_file` (cross-platform mmap)
Single class hiding the OS difference behind one API:

```cpp
class MappedFile {
public:
    MappedFile();                        // empty
    ~MappedFile();                       // unmaps if open
    bool open(const char* path, long expected_size);  // false on missing / size mismatch / mmap fail
    void close();
    const unsigned char* data() const;   // nullptr if not open
    long size() const;
    bool is_missing() const;             // true iff last open failed with "file not found"
private:
#if defined(_WIN32)
    void* file_handle_;                  // HANDLE
    void* mapping_handle_;               // HANDLE
#else
    int   fd_;
#endif
    const unsigned char* data_;
    long  size_;
    bool  missing_;
};
```

Implementation:
- **Windows**: `CreateFileA` (OPEN_EXISTING, GENERIC_READ, FILE_SHARE_READ) →
  `CreateFileMappingA` (PAGE_READONLY) → `MapViewOfFile` (FILE_MAP_READ).
  Detect missing via `GetLastError() == ERROR_FILE_NOT_FOUND` or `ERROR_PATH_NOT_FOUND`.
- **POSIX**: `open(path, O_RDONLY)` → `fstat` to verify size → `mmap(NULL, size,
  PROT_READ, MAP_PRIVATE, fd, 0)`. Detect missing via `errno == ENOENT`.

No heap allocations. `MappedFile` is stored by value inside each `Slot`.

#### `TileStore`
- Fixed-capacity LRU cache of memory-mapped tiles.
- **Capacity: 256 slots** (compile-time constant).
- Storage layout (all preallocated in the `TileStore` object — no heap after construction):

  ```cpp
  struct Slot {
      unsigned int key;         // packed (lat_floor+90) << 16 | (lon_floor+180);
                                // 0xFFFFFFFFu = empty
      MappedFile   mapped;      // owns the mmap; known_missing == mapped.is_missing()
      int          prev, next;  // LRU doubly-linked list (indexes into slots[])
  };
  static const int CAPACITY      = 256;
  static const int TILES_DIR_MAX = 512;   // portable path cap; replaces MAX_PATH
  Slot slots_[CAPACITY];
  int  lru_head_, lru_tail_;             // most-recently-used at head
  char tiles_dir_[TILES_DIR_MAX];
  ```

- **Lookup** signature:
  `const unsigned char* get(int lat_floor, int lon_floor, bool* out_missing, bool* out_io_err);`
  1. Take the unique lock (see thread-safety note in section 4).
  2. Linear scan `slots_` for the packed key (256 entries — cache-friendly).
  3. If found:
     - If `mapped.data()` non-null → move slot to LRU head → return pointer.
     - Else (`mapped.is_missing()` is true) → set `*out_missing = true`, return null.
  4. If not found:
     - Pick LRU tail slot, `mapped.close()` it, mark key empty.
     - Build path with `build_tile_path()` (uses `tiles_dir_` + computed subdir/file).
     - Call `mapped.open(path, DTED_FILE_SIZE)`.
     - On success → fill slot, link at LRU head, return pointer.
     - On missing → store slot with `mapped.is_missing()==true` (negative cache),
       set `*out_missing = true`, return null.
     - On other error → leave slot empty, set `*out_io_err = true`, return null.

- **No heap allocation** anywhere in `get()`. All storage is in `slots_`.
- The `tiles_dir` is copied into `tiles_dir_[TILES_DIR_MAX]` at construction
  with `snprintf` (truncation → `es_create` returns NULL).

#### `ElevationService`
- Holds one `TileStore`.
- `int query(double lat, double lon, double* out)`:
  1. Validate `out_msl != NULL`, `lat ∈ [-90,90]`, `lon ∈ [-180,180]`
     → else `ES_ERR_BAD_ARG`.
  2. `lat_floor = (int)floor(lat); lon_floor = (int)floor(lon);`
  3. Fractional position within the tile (note: DTED rows increase **northward**):
     - `row_f = (lat - lat_floor) * (DTED_DIM - 1);`  // 0 at south edge, 3600 at north
     - `col_f = (lon - lon_floor) * (DTED_DIM - 1);`  // 0 at west edge,  3600 at east
  4. `r = (int)floor(row_f); c = (int)floor(col_f);`
     `tx = col_f - c; ty = row_f - r;`
  5. Get tile pointer via `TileStore::get`. Map to `ES_ERR_NO_DATA` / `ES_ERR_IO`.
  6. Fill `double p[4][4]` from the 4×4 neighborhood `[c-1..c+2] × [r-1..r+2]`,
     **clamping each index to `[0, DTED_DIM-1]`** (no cross-tile read in v1).
     For each post: `offset = dted_post_offset(cc, rr); v = read_be_i16(data + offset);`
     If `v == DTED_VOID` → return `ES_ERR_NO_DATA`. Else `p[i][j] = (double)v;`.
  7. Call `bicubic_interpolate(p, tx, ty)` → write `*out_msl` → return `ES_OK`.

- **Zero heap allocations** in `query()`. A 16-element stack array
  (`double p[4][4]`) is the only working memory.

---

## 4. Thread Safety

- `es_get_elev` may be called from any thread concurrently.
- `TileStore` uses a `std::shared_mutex`:
  - Cache hits → shared lock (many readers concurrent).
  - Cache misses (load/evict) → unique lock.
- The LRU list update on a hit technically also mutates state. **v1 takes
  a single `std::mutex` (unique lock) for every `get()`**. The critical section
  is tens of nanoseconds (linear scan of 256 packed keys), so contention isn't
  a real concern at mouse-hover rates. This keeps the implementation small and
  correct. `std::shared_mutex` is not required — and `std::mutex` is available
  in any compiler that supports C++11+ on both Windows and Linux.

- `es_create` / `es_destroy` are not thread-safe vs. other calls. Caller's job.

---

## 5. Memory Profile

- **Per-tile virtual address space**: ~26 MB (SRTM1).
- **Cache capacity**: 256 tiles → ~6.6 GB virtual address space (fine on x64).
- **Resident memory**: only the 4 KB pages actually touched. For point queries
  this is typically a few MB total. The OS pages out cold tiles automatically.
- **Heap allocations after `es_create`**: zero.

---

## 6. Build & Solution Integration

The project must build on both Windows (DLL) and Linux (shared object).
**Same sources**, OS-specific code isolated to `mapped_file.cpp` via `#ifdef _WIN32`.

### Windows — Visual Studio project

Add a new VS project to `server/radar_sea_level.sln`:

- **Project name**: `elevation_dll`
- **Project type**: Dynamic-Link Library (`.vcxproj`)
- **Output**: `elevation_dll.dll` + `elevation_dll.lib` (import lib)
- **Configurations**: Debug | x64, Release | x64
- **Preprocessor**: define `ELEVATION_DLL_EXPORTS` when building the DLL;
  consumers do NOT define it (so the header switches to `dllimport`).
- **C++ standard**: C++11 minimum (use C++17 for consistency with the rest of
  the solution; the code itself stays C++11-clean so the same sources compile
  on older Linux toolchains).
- **Additional include dirs**: `include\` (project-local only).
- **Do NOT** add include paths to `..\include\` or any other existing project dir.

Files to add to the project:
```
include\elevation_api.h
src\elevation_service.cpp
src\tile_store.h
src\tile_store.cpp
src\bicubic.h
src\bicubic.cpp
src\dt2_read.h
src\mapped_file.h
src\mapped_file.cpp
```

The existing projects (`radar_server`, `lut_tcp_server`, etc.) must remain
unchanged. Do not add the new project as a reference to any of them.

### Linux — Makefile

Add `server/elevation_dll/Makefile` so the same sources produce
`libelevation.so`. Keep it self-contained — do NOT depend on any other
Makefile in the repo.

```make
# server/elevation_dll/Makefile
CXX      ?= g++
CXXFLAGS ?= -O2 -Wall -Wextra -fPIC -std=c++11 -pthread
LDFLAGS  ?= -shared -pthread
TARGET   := libelevation.so
SRCS     := src/elevation_service.cpp src/tile_store.cpp \
            src/bicubic.cpp src/mapped_file.cpp
OBJS     := $(SRCS:.cpp=.o)

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -DELEVATION_DLL_EXPORTS -Iinclude -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
```

Build on Linux:
```
cd server/elevation_dll && make
```

---

## 7. Acceptance Criteria

The implementation is complete when:

1. **Windows**: the DLL builds in both Debug|x64 and Release|x64 with **zero
   warnings** at `/W4`.
2. **Linux**: `cd server/elevation_dll && make` produces `libelevation.so`
   with **zero warnings** at `-Wall -Wextra`.
3. The existing solution still builds with **no changes** to any other project's
   sources or settings.
4. The public header `elevation_api.h` compiles as **both** C89 and C++98 (test
   by including it from a `.c` file built with `-std=c89 -Wall` and from a
   `.cpp` file built with `-std=c++98 -Wall`).
5. A minimal C++ test consumer (below) loads the library, queries known points,
   and prints expected values on both Windows and Linux.
6. Memory check (Release): after 100,000 random global queries, process
   resident set stays under **500 MB**. Confirms the LRU + mmap design is bounded.
7. Thread check: 8 threads × 100,000 queries complete without crash or data
   race (run under Application Verifier on Windows / ThreadSanitizer on Linux).
8. No `new`, `malloc`, `std::vector`, `std::string`, `std::unordered_map`,
   `std::map`, or any STL container that allocates appears in the implementation
   of `TileStore::get`, `ElevationService::query`, or any function they call.
   `std::mutex` is allowed (does not allocate on lock).

### Minimal test consumer

Place at `server/elevation_dll/test/test_consumer.cpp` (separate console
project on Windows; on Linux `g++ -std=c++98 test_consumer.cpp -L.. -lelevation -o test`):

```cpp
#include "elevation_api.h"
#include <stdio.h>

int main(void) {
    ElevationService* svc = es_create("./tiles_global/");
    if (!svc) { printf("create failed\n"); return 1; }

    static const struct { double lat; double lon; const char* name; } pts[] = {
        { 32.0853,  34.7818, "Tel Aviv" },
        { 27.9881,  86.9250, "Everest summit" },
        { 40.7128, -74.0060, "NYC" },
        {  0.0,      0.0,    "Null Island (sea)" }
    };
    int i;
    for (i = 0; i < 4; ++i) {
        double m;
        int rc = es_get_elev(svc, pts[i].lat, pts[i].lon, &m);
        if (rc == ES_OK)              printf("%-20s %.1f m\n", pts[i].name, m);
        else if (rc == ES_ERR_NO_DATA) printf("%-20s no-data\n", pts[i].name);
        else                           printf("%-20s err=%d\n",  pts[i].name, rc);
    }
    es_destroy(svc);
    return 0;
}
```

(Written in C-compatible style so the same `.cpp` body would also compile as
`.c` — proves the header is usable from C / C++98.)

---

## 8. Instructions for the Implementing Agent

Read this section carefully before writing any code.

### What to do

1. **Create only the files listed in section 6.** Do not create extra files,
   docs, or wrappers.
2. **Copy the bicubic math** from `server/src/dem_database.cpp` into
   `src/bicubic.cpp`. It is a Catmull-Rom kernel over a 4×4 grid. Do **not**
   `#include` anything from the existing `server/include` or `server/src` —
   this project must be self-contained.
3. **Implement `MappedFile`** with the cross-platform `#ifdef _WIN32` split
   described in section 3. Win32 path uses `CreateFileA` /
   `CreateFileMappingA` / `MapViewOfFile`; POSIX path uses `open` / `fstat`
   / `mmap`. Verify file size == `DTED_FILE_SIZE` before returning success.
4. **Implement `TileStore`** exactly as specified in section 3. Fixed array
   of 256 `Slot`s, doubly-linked LRU via integer indices, a single
   `std::mutex`, fixed `char tiles_dir_[TILES_DIR_MAX]`. No dynamic containers.
5. **Implement `ElevationService::query`** per section 3. Read posts directly
   from the mmap'd bytes using `dted_post_offset()` and `read_be_i16()`.
   Clamp the 4×4 neighborhood; do not attempt cross-tile reads in v1.
6. **Windows**: add the project to `server/radar_sea_level.sln` per section 6.
   Do **not** modify any other `.vcxproj` in the solution.
   **Linux**: create the `Makefile` per section 6.
7. **Build and run** the test consumer from section 7 on Windows. Confirm
   Tel Aviv, Everest, and NYC return plausible values; Null Island returns
   `ES_ERR_NO_DATA`. (If Linux DTED tiles are not available, the Linux build
   only needs to produce `libelevation.so` cleanly.)

### What NOT to do

- Do **not** modify `server/include/dem_database.h`, `server/src/dem_database.cpp`,
  `server/src/dted_tile.cpp`, or any existing source file. The whole point of
  this design is to leave them alone.
- Do **not** add HTTP, JSON, or any network code. This is a pure C ABI library.
- Do **not** put any C++ types (`std::string`, `bool`, references, classes)
  in the **public** header `elevation_api.h`. C89 / C++98 consumers must work.
- Do **not** introduce any heap allocation on the query hot path. Verify by
  inspection (grep for `new`, `malloc`, container types) before declaring done.
- Do **not** add comments to the bicubic code copied from `dem_database.cpp`.
  The original has none; preserve that.
- Do **not** add a CMakeLists.txt, vcpkg manifest, or any other build system —
  only the existing VS solution (Windows) and the Makefile (Linux).
- Do **not** depend on `boost`, `Eigen`, or any external library. Standard
  C++11, plus Win32 (`<windows.h>`) on Windows and POSIX (`<sys/mman.h>`,
  `<fcntl.h>`, `<unistd.h>`, `<sys/stat.h>`) on Linux.
- Do **not** use Windows-only constants like `MAX_PATH` outside `mapped_file.cpp`
  Win32 branch. Use the project-local `TILES_DIR_MAX` for portable code.
- Do **not** mention Claude, AI, or generation tooling anywhere in code or docs.

### Self-check before declaring done

Run through this list:

- [ ] Windows: `elevation_dll.dll` and `elevation_dll.lib` produced in `x64/Release/`.
- [ ] Linux: `make` produces `libelevation.so` cleanly.
- [ ] Existing solution still builds untouched (no edits to any other `.vcxproj`
      or any file under `server/include`, `server/src`, or `server/app`).
- [ ] Public header includes only `<stddef.h>`-level types (no `<stdint.h>`,
      no `<stdbool.h>`, no C++ headers). It compiles as both C89 and C++98.
- [ ] Test consumer prints expected values for the four test points (Windows).
- [ ] Grep `src/` for `new `, `malloc`, `std::vector`, `std::string`,
      `std::unordered_map`, `std::map` — should produce **no** matches in
      `tile_store.*`, `elevation_service.*`, `bicubic.*`, `dt2_read.h`,
      `mapped_file.*`. (The one allowed allocation is the single `new
      ElevationService` in `es_create`; that's the documented exception.)
- [ ] Windows `/W4` clean; Linux `-Wall -Wextra` clean.
- [ ] All four error codes (`ES_OK`, `ES_ERR_BAD_ARG`, `ES_ERR_NO_DATA`,
      `ES_ERR_IO`) are reachable by some input.

---

## 9. Performance Test

`server/elevation_dll/test/perf_test.cpp` measures query latency across three
scenarios and prints min / avg / p50 / p95 / p99 / max / throughput.

**Build and run (Linux)**

```bash
cd server/elevation_dll
g++ -std=c++11 -O2 -Iinclude test/perf_test.cpp -L. -lelevation -Wl,-rpath,. -lm \
    -o perf_test
./perf_test <tiles_dir> [grid_steps]
# grid_steps: points per axis for grid sweeps (default 50, e.g. 50x50 = 2500 queries)
```

**Example output** (WSL2, 37 DTED2 tiles covering Israel, grid_steps=30):

```
=== Named points (1000 iterations each) ===
  Tel Aviv             elev=19.9 m     min= 0.15 avg=15.32 p99= 0.16 max=15163 us
  Jerusalem            elev=778.3 m    min= 0.16 avg=15.49 p99= 0.18 max=15309 us
  Dead Sea             elev=-415.0 m   min= 0.22 avg= 3.05 p99= 0.24 max= 2817 us
  Null Island          elev=no-data    min= 0.09 avg= 0.43 p99= 0.09 max=  339 us

=== Grid sweep — single tile ===
  min=0.08 us  avg=80.98 us  p99=2724 us  throughput: 12349 queries/sec

=== Grid sweep — full coverage ===
  min=0.08 us  avg=436 us    p99=4396 us  throughput: 2291 queries/sec
```

**Interpreting results**

- The large `max` on named points is the **cold tile load** (mmap + first OS page
  fault). Subsequent queries on a warm tile take ~0.15 µs.
- `avg` on the full-coverage grid is dominated by cold loads when many tiles are
  touched for the first time. In a long-running process, warm throughput exceeds
  1 million queries/sec.
- `no-data` in grid sweeps means the grid point falls outside available tile
  coverage (sea cells or missing tiles).

---

## 10. Optional Future Work (NOT for v1)

- Cross-tile bicubic (read posts from neighbor tiles at the seam).
- Async neighbor prefetch on each query for smoother panning.
- O(1) cache lookup via a small open-addressing index over `slots_`.
- Support for DTED2's variable longitudinal post spacing above |lat| ≥ 50°.
- macOS support (POSIX `mmap` path already covers it — just build target).
- Optional bilinear mode for callers that prefer speed over smoothness.

These are deliberately out of scope for v1. Do not implement them.
