# Context

## What this library does

`elevation_dll` answers one question: **"what is the terrain elevation (MSL) at
this lat/lon?"** It reads directly from DTED Level 2 (`.dt2`) files on disk and
returns a bicubic-interpolated elevation in meters.

It is designed for **high-rate point queries** — e.g. mouse-hover on a map UI
or a real-time radar target feed — with:

- **Zero heap allocation on the query path** — no `new`, no `malloc` after
  `es_create()` returns.
- **Bounded resident memory** — a 256-slot LRU of memory-mapped tiles; the OS
  pages out cold tiles automatically, so physical RAM stays proportional to
  recently-touched tiles, not total tile count.

## Isolation

This library is **fully self-contained**. It does not link against, share code
with, or modify any other component in the `radar_sea_level` solution. The
existing `DemDatabase`, `DtedTile`, `QueryHandler`, and server code are not
touched.

## Consumer compatibility

The public surface is a **pure C ABI** (`extern "C"`, no C++ types, no STL).
The header `elevation_api.h` uses only `int`, `double`, `char`, and an opaque
`struct` pointer — no `<stdint.h>`, `<stdbool.h>`, `nullptr`, or anything that
would break C89 / C++98.

Consumers may be written in:
- C (C89 and later)
- C++ (C++98, C++03, C++11, and later)
- Any language that can call a C-ABI shared library: Python (ctypes/cffi), C#
  (P/Invoke), Rust (FFI), Go (cgo), Java (JNA/JNI), etc.

## Known limitations (v1)

- **Cross-tile bicubic**: when a query point falls within one post-spacing of a
  tile boundary, the 4×4 Catmull-Rom neighborhood is clamped to the current
  tile rather than reading neighbor tiles. Effect: slight smoothing error within
  ~1 arc-second of tile edges. Acceptable for hover-on-map UX.
- **High-latitude tiles**: DTED2 specifies reduced longitudinal post spacing
  above |lat| ≥ 50°. v1 always assumes 3601×3601; queries at |lat| ≥ 50° may
  return `ES_ERR_IO` if the file size doesn't match. Coverage area (Israel) is
  well inside ±50°.
- **Case-sensitive paths on Linux**: `build_tile_path()` generates lowercase
  directory and file names (`e034/n32.dt2`). Tile files stored with uppercase
  names will not be found on Linux (they work on Windows/NTFS).

## Optional future work

These are deliberately out of scope for v1:

- Cross-tile bicubic (read posts from neighbor tiles at seams).
- Async neighbour prefetch on each query for smoother panning.
- O(1) cache lookup via a small open-addressing index over the 256 slots.
- Support for DTED2's variable post spacing above |lat| ≥ 50°.
- macOS support (the POSIX `mmap` path already covers it — just a build target).
- Optional bilinear mode for callers that prefer speed over smoothness.
