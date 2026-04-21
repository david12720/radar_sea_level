# Changelog

## [Unreleased]

### Added
- HTTP REST server (`main_server.cpp`) exposing `POST /query` and `GET /health` endpoints
- `app/query_handler` — pure domain layer (RadarQuery → TargetResult, no HTTP dependency)
- `app/radar_server` — HTTP transport layer using cpp-httplib (header-only)
- Server-side input validation with structured JSON error responses (HTTP 400 / 422 / 500)
- CLI arguments for radar position and server config: `--lat`, `--lon`, `--alt`, `--port`, `--tiles`, `--max-range`
- Python GUI (`gui/`) — Dash + dash-leaflet interactive map
  - Offline map tiles via local MBTiles file (no internet required at runtime)
  - Target pins color-coded by AGL (red / orange / yellow / green)
  - Max-range coverage circle around radar position
  - Startup health check with clear error banner if server is unreachable
- `map_tiles/download_tiles.py` — parallel tile downloader for offline map setup
  - `--max-zoom` argument to control highest zoom level downloaded (default: 16)
  - `--bbox west south east north` argument for custom region (default: Israel)
  - Resume support: re-running skips already-downloaded tiles instead of restarting
  - Single persistent thread pool (no per-batch executor overhead, no inter-batch sleep)
- C++ unit tests for `QueryHandler` (`tests/test_query_handler.cpp`)
- Python integration tests for `RadarApiClient` (`gui/tests/test_api_client.py`)
- Vendor headers: `cpp-httplib`, `nlohmann/json` (both header-only)
- `CLAUDE.md` — project context for development sessions
- `PLAN_GUI.md` — GUI architecture and implementation plan

## [0.3.0] — 2026-04-20

### Added
- `DemDatabase::loadTilesAround()` — automatic tile loading based on radar position and max range
- No more hardcoded lat/lon loops; tile coverage computed from radar position at startup

## [0.2.0] — 2026-04-20

### Added
- Multi-file project structure (include/, src/, CMakeLists.txt)
- SRTM1 / SRTM3 auto-detection from file size
- `DtedTile` stores its own dimensions (supports mixed tile resolutions)
- Bicubic (Catmull-Rom) interpolation in `DemDatabase`
- `ElevationLUT` pre-computed range/azimuth table — O(1) per-target lookup
- `LutConfig` struct for configurable range/azimuth resolution
- Two `computeTargetSeaLevel()` overloads: direct DEM and LUT

## [0.1.0] — 2026-04-20

### Added
- Initial implementation: flat-Earth geometry, SRTM HGT loading, AGL computation
