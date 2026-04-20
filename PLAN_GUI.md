# GUI Service Plan — Radar Sea Level Viewer

> **Status: IMPLEMENTED** — all 9 steps complete as of 2026-04-20.


## Goal

A separate GUI service that displays a live map around the radar position. The user submits radar measurements (range, azimuth, elevation) one at a time; each submission plots the target on the map with its lat/lon position and altitude above sea level. Multiple targets accumulate as pins; a "Clear" button resets the map.

## Architecture

```
C++ REST Server                          Python Dash GUI
─────────────────────────────────────    ────────────────────────────────────
core/          (existing, untouched)     api_client.py   — HTTP calls only
  radar_compute.cpp                      map_view.py     — Dash map component
  dem_database.cpp                       controls.py     — input panel
  elevation_lut.cpp                      app.py          — wires + runs Dash

app/           (new)
  query_handler.h/.cpp  — pure logic
  radar_server.h/.cpp   — HTTP only
main_server.cpp         — startup + wiring
```

**Key principle:** HTTP is a transport detail. `query_handler` has zero HTTP knowledge. `radar_server` has zero domain knowledge. Python `api_client.py` is the only file that knows the server URL.

## C++ Side — New Files

### `app/query_handler.h`
```cpp
struct RadarQuery {
    double range_m, azimuth_deg, elevation_deg;
};

class QueryHandler {
public:
    QueryHandler(const LLA& radar, const LutConfig& cfg, const std::string& tiles_dir);
    TargetResult handle(const RadarQuery& q) const;
private:
    ElevationLUT lut_;
    LLA radar_;
};
```

### `app/radar_server.h`
```cpp
class RadarServer {
public:
    RadarServer(QueryHandler& handler, int port);
    void start();   // blocking, Ctrl-C to terminate in v1
private:
    QueryHandler& handler_;
    int port_;
};
```
- POST `/query` → JSON in, JSON out
- GET  `/health` → `{"status":"ok"}`

### Error handling contract
Server is the single source of truth for validation. Dash sliders set UX bounds but do not duplicate server-side checks.

| Condition | HTTP | Response body |
|-----------|------|---------------|
| Happy path | 200 | `{ lat_deg, lon_deg, alt_msl_m, ground_elev_m, agl_m, horiz_range_m }` |
| Missing / wrong-type JSON fields | 400 | `{ "error": "bad request: <detail>" }` |
| Out-of-range input (range, azimuth, elevation) | 400 | `{ "error": "validation failed: <field> <reason>" }` |
| Target lat/lon outside loaded tile coverage | 422 | `{ "error": "no DEM coverage at target location" }` |
| Any other exception | 500 | `{ "error": "internal server error" }` |

Server-side validation bounds:
- `range_m` ∈ [0, max_range_m]  (max_range_m from CLI)
- `azimuth_deg` ∈ [0, 360)
- `elevation_deg` ∈ [-10, +45]

### `main_server.cpp`
- Parse CLI args: `--lat 32.08 --lon 34.76 --alt 10 --port 8080 --tiles ./tiles/ --max-range 50000`
- All args have defaults; radar position is fully configurable at startup
- Construct `QueryHandler`, construct `RadarServer`, call `start()`

### JSON contract (POST `/query`)
```json
// request
{ "range_m": 8000, "azimuth_deg": 45.0, "elevation_deg": -0.5 }

// response
{
  "lat_deg": 32.091,
  "lon_deg": 34.753,
  "alt_msl_m": 35.2,
  "ground_elev_m": 0.0,
  "agl_m": 35.2,
  "horiz_range_m": 7990.1
}
```

### Dependencies (header-only, no build changes needed)
- [cpp-httplib](https://github.com/yhirose/cpp-httplib) — single header `httplib.h`
- [nlohmann/json](https://github.com/nlohmann/json) — single header `json.hpp`

Place both in `include/vendor/`.

## Python Side — New Files

### `gui/api_client.py`
```python
class RadarApiClient:
    def __init__(self, base_url: str = "http://localhost:8080"):
        ...
    def query(self, range_m, azimuth_deg, elevation_deg) -> dict:
        # POST /query, return parsed JSON dict
    def health(self) -> bool:
        # GET /health
```

### `gui/controls.py`
- Range slider: 0–50 000 m
- Azimuth input: 0–360°
- Elevation input: -10° to +45°
- "Add Target" button
- "Clear All" button

### `gui/tile_server.py`
- Serves map tiles from a local MBTiles file over HTTP (e.g. `http://localhost:8081/{z}/{x}/{y}.png`)
- Runs as a background thread inside the same Python process
- Tile file: download once, ship with the application (not committed to git — binary, ~300 MB)
- Recommended source: [download.geofabrik.de](https://download.geofabrik.de) or OpenMapTiles export for Israel/Middle East region
- No internet needed at runtime

### `gui/map_view.py`
- `dash-leaflet` map centered on radar position
- Tile URL points to local tile server (`http://localhost:8081/{z}/{x}/{y}.png`)
- Radar position shown as a fixed marker (distinct icon/color, e.g. blue crosshair)
- **Max-range circle**: faint translucent circle of radius `max_range_m` around radar marker, to visualize coverage bounds
- Each target = pin color-coded by AGL:
  - **Red** — AGL < 50 m (near ground / sea skimmer)
  - **Orange** — AGL 50–300 m (low altitude)
  - **Yellow** — AGL 300–1000 m (medium)
  - **Green** — AGL > 1000 m (high altitude)
- Popup on click: lat/lon, alt MSL, AGL, slant range, azimuth

### `gui/app.py`
- Wires controls + map + api_client + tile_server
- Starts tile server thread at launch
- **Startup health check**: calls `api_client.health()` on launch; if unreachable, displays a clear error banner ("Radar server not reachable at <url>") instead of the map
- Stores target list in `dcc.Store` (client-side state — refresh = lose list; acceptable for v1)
- Callback: "Add Target" → POST to C++ server → append to store → redraw map
- Error handling: if server returns 4xx/5xx, show error toast; do not append target

### `requirements.txt` (gui/)
```
dash
dash-leaflet
requests
sqlite3       # stdlib — for reading MBTiles (SQLite format)
Pillow        # for serving tile images from MBTiles
flask         # tile server (lightweight, already a Dash dependency)
```

## Implementation Order

1. **C++ `query_handler`** — pure logic, no HTTP
2. **C++ unit tests for `query_handler`** — sea target, elevated terrain, out-of-coverage, boundary inputs
3. **C++ `radar_server`** — HTTP wrapper + error contract, test with `curl`
4. **`main_server.cpp`** — startup + CLI args, verify end-to-end with curl
5. **Download MBTiles** — get offline map file, verify it opens
6. **Python `tile_server.py`** — serve MBTiles locally, verify tiles load in browser
7. **Python `api_client.py`** — verify it hits the C++ server (with a small pytest)
8. **Python `controls.py` + `map_view.py`** — UI layout with offline map + max-range circle
9. **Python `app.py`** — wire callbacks + startup health check, test full flow

## Build Changes

Add to `CMakeLists.txt`:
```cmake
add_executable(radar_server
    main_server.cpp
    app/query_handler.cpp
    app/radar_server.cpp
    src/dted_tile.cpp
    src/dem_database.cpp
    src/elevation_lut.cpp
    src/radar_compute.cpp
)
target_include_directories(radar_server PRIVATE include)
target_compile_features(radar_server PRIVATE cxx_std_17)

# Unit tests (step 2)
add_executable(test_query_handler
    tests/test_query_handler.cpp
    app/query_handler.cpp
    src/dted_tile.cpp
    src/dem_database.cpp
    src/elevation_lut.cpp
    src/radar_compute.cpp
)
target_include_directories(test_query_handler PRIVATE include)
target_compile_features(test_query_handler PRIVATE cxx_std_17)
```

Keep test framework minimal — no GoogleTest dependency. A tiny `ASSERT_NEAR` macro + `main()` returning non-zero on failure is enough.

## Folder Structure After Implementation

```
radar_sea_level/
├── include/
│   ├── vendor/
│   │   ├── httplib.h
│   │   └── json.hpp
│   └── ... (existing)
├── app/
│   ├── query_handler.h
│   ├── query_handler.cpp
│   ├── radar_server.h
│   └── radar_server.cpp
├── gui/
│   ├── api_client.py
│   ├── tile_server.py
│   ├── controls.py
│   ├── map_view.py
│   ├── app.py
│   ├── requirements.txt
│   └── tests/
│       └── test_api_client.py
├── tests/
│   └── test_query_handler.cpp
├── map_tiles/               # gitignored — MBTiles file lives here
│   └── israel.mbtiles       # download once, ~300 MB
├── src/         (existing)
├── main.cpp     (existing demo, unchanged)
├── main_server.cpp  (new)
└── CMakeLists.txt   (updated)
```

## Decisions (resolved)

| Decision | Choice |
|----------|--------|
| Radar position | CLI args: `--lat --lon --alt` with defaults |
| Map tiles | Local MBTiles file served by `tile_server.py` — fully offline |
| Target pin color | AGL-coded: red < 50 m, orange < 300 m, yellow < 1000 m, green ≥ 1000 m |
| C++ server port | `--port 8080` (default) |
| Tile server port | `8081` (internal, fixed) |

## Offline Map Setup (one-time)

Download an MBTiles file for the coverage area and place in `map_tiles/`:

```bash
# Israel + surroundings — free export from OpenMapTiles or BBBike.org
# ~100–400 MB depending on zoom level depth
```

Add `map_tiles/` to `.gitignore`.
