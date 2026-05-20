# Testing

## Acceptance criteria

The implementation is complete when:

1. **Windows**: `elevation_dll.dll` and `.lib` build in all four configurations
   (Debug|x64, Release|x64, Debug|Win32, Release|Win32) with **zero warnings** at `/W4`.
2. **Linux**: `make` produces `libelevation.so` with **zero warnings** at
   `-Wall -Wextra`.
3. The existing solution still builds with **no changes** to any other project's
   sources or settings.
4. The public header `elevation_api.h` compiles as both C89 and C++98.
5. `poc_consumer.exe` returns plausible values for the preset locations (see
   below) on both x64 and Win32.
6. Memory check: after 100,000 random queries, process resident set stays under
   **500 MB**. Confirms the LRU + mmap design is bounded.
7. Thread check: 8 threads × 100,000 queries complete without crash or data
   race (Application Verifier on Windows / ThreadSanitizer on Linux).
8. No `new`, `malloc`, `std::vector`, `std::string`, `std::unordered_map`, or
   `std::map` appears in `TileStore::get`, `ElevationService::query`, or any
   function they call. (`new ElevationService` inside `es_create` is the one
   allowed exception.)

### Expected values (Israel coverage, 37 DTED2 tiles)

| Location | Expected elevation |
|----------|--------------------|
| Tel Aviv (32.0853, 34.7818) | ~20 m |
| Jerusalem (31.7683, 35.2137) | ~780 m |
| Haifa (32.8156, 35.1072) | ~10 m |
| Dead Sea (31.5000, 35.4500) | ~−415 m |
| Null Island (0.0, 0.0) | `ES_ERR_NO_DATA` (sea) |

---

## Performance test

`test/perf_test.cpp` measures query latency across three scenarios.

### Build and run (Linux)

```bash
cd server/elevation_dll
g++ -std=c++11 -O2 -Iinclude test/perf_test.cpp -L. -lelevation -Wl,-rpath,. -lm \
    -o perf_test
./perf_test <tiles_dir> [grid_steps]
# grid_steps: points per axis for grid sweeps (default 50)
```

### Build and run (Windows)

Build `poc_consumer` in Release|x64 via Visual Studio, then add `perf_test.cpp`
to the `poc_consumer` project (or create a separate project following the same
`.vcxproj` pattern).

### Example output (WSL2, 37 DTED2 tiles, grid_steps=30)

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

### Interpreting results

- The large `max` on named points is the **cold tile load** — mmap + first OS
  page fault. Subsequent queries on a warm tile take ~0.15 µs.
- `avg` on the full-coverage grid is dominated by cold loads when many tiles are
  touched for the first time. In a long-running process, warm throughput exceeds
  1 million queries/sec.
- `no-data` in grid sweeps means the point falls outside available tile
  coverage (sea cells or missing tiles).
