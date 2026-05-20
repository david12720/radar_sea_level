# elevation_dll — Global DTED2 Elevation Lookup

Standalone shared library (Windows `.dll` / Linux `.so`) for high-rate terrain
elevation (MSL) queries from on-disk DTED Level 2 tiles. Zero heap allocation on
the query path; bounded resident memory regardless of tile count.

## Documentation

| Document | Contents |
|----------|----------|
| [QUICKSTART.md](QUICKSTART.md) | Build, tile layout, usage in C / C++ / Python / C# |
| [CONTEXT.md](CONTEXT.md) | Purpose, design decisions, consumer compatibility, future scope |
| [ARCHITECTURE.md](ARCHITECTURE.md) | Internal components, DTED2 format, thread safety, memory profile |
| [BUILD.md](BUILD.md) | VS solution integration, Makefile reference |
| [TESTING.md](TESTING.md) | Acceptance criteria, performance test |

## One-line build

**Windows** — open `server/radar_sea_level.sln`, build `elevation_dll` in
Release | x64 (or Release | Win32).

**Linux**

```bash
cd server/elevation_dll && make
```
