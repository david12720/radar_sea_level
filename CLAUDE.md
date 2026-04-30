# radar_sea_level — Claude Code Context

## What This Project Does

Computes **terrain elevation (MSL)** beneath a radar target and the target's **height above ground level (AGL)**.

Input: radar position (LLA) + measurement (slant range, azimuth, elevation angle)  
Output: target lat/lon, altitude MSL, terrain MSL, AGL

Also provides **UTM ↔ lat/lon coordinate conversion** (WGS84, zone auto-detected from longitude).

## Build

Compiler is **WSL g++** (no Windows compiler). cmake is not installed — build directly:

```bash
wsl bash -c "cd /mnt/c/Users/david/Downloads/radar_sea_level/server && g++ -std=c++17 -O2 -Iinclude -Iapp -o radar_server main_server.cpp app/query_handler.cpp app/radar_server.cpp src/coord_convert.cpp src/dted_tile.cpp src/dem_database.cpp src/elevation_lut.cpp src/radar_compute.cpp src/relative_angle.cpp -lpthread"
```

Run the server (from `server/` so `./tiles/` resolves):
```bash
wsl bash -c "cd /mnt/c/Users/david/Downloads/radar_sea_level/server && ./radar_server [--port 8080] [--tiles ./tiles/] [--max-range 50000]"
```

Run the GUI:
```bash
python gui/app.py [--server http://localhost:8080] [--tiles gui/map_tiles/israel.mbtiles]
```

## Project Structure

```
server/
  include/
    constants.h        — DEG2RAD, RAD2DEG, EARTH_RADIUS, SRTM1/SRTM3 dimensions
    types.h            — LLA, RadarMeasurement, TargetResult, GroundPoint
    coord_convert.h    — UtmPoint struct, ll_to_utm(), utm_to_ll() (WGS84 UTM)
    dted_tile.h        — DtedTile (stores cols, rows, post_spacing per instance)
    dem_database.h     — DemDatabase (multi-tile manager, bicubic interpolation)
    elevation_lut.h    — LutConfig, ElevationLUT (pre-computed range/az table)
    radar_compute.h    — computeTargetSeaLevel() (two overloads)
    relative_angle.h   — elevationToGround(), relativeElevation()
    vendor/httplib.h   — cpp-httplib (header-only HTTP server/client)
    vendor/json.hpp    — nlohmann/json (header-only JSON)
  src/
    coord_convert.cpp  — UTM ↔ LL conversion (Snyder series, WGS84)
    dted_tile.cpp      — loadSRTM (auto-detects SRTM1/SRTM3 by file size), loadDTED2
    dem_database.cpp   — bicubicInterpolate, loadTile, getElevation, loadTilesAround
    elevation_lut.cpp  — build(), lookup()
    radar_compute.cpp  — groundPoint(), computeGeometry(), computeTargetSeaLevel()
    relative_angle.cpp — elevationToGround(), relativeElevation()
  app/
    query_handler.h/.cpp  — pure domain logic: RadarQuery → TargetResult, owns DemDatabase
    radar_server.h/.cpp   — HTTP transport: JSON ↔ QueryHandler, registers all endpoints
    lut_tcp_server.h/.cpp — TCP variant using ElevationLUT (O(1) per query)
    lut_exporter.h/.cpp   — exports LUT to file
    tdp_dtm_tcp_server.h/.cpp — TCP server: UTM input → terrain MSL response + output.bin LUT
    target_tcp_server.h/.cpp — TCP target server
  main.cpp            — demo CLI: load tiles, build LUT, run test queries
  main_server.cpp     — HTTP server entry point (parses --port/--tiles/--max-range)
  main_lut_server.cpp — LUT server entry point
  main_tdp_dtm_server.cpp — TDP DTM TCP server entry point
  main_target_server.cpp — target server entry point
  tests/
    test_query_handler.cpp — unit tests for QueryHandler
  tiles/              — .hgt files (gitignored, SRTM3 1201×1201, Israel coverage)

gui/
  app.py              — Dash entry point; wires tile server, api client, controls, map;
                        all Dash callbacks live here
  api_client.py       — RadarApiClient: HTTP wrapper for all C++ server endpoints;
                        also standalone helpers: ping_open_elevation, get_open_elevation,
                        get_open_meteo_elevation
  controls.py         — left panel layout (radar form, target sliders, coord converter);
                        no server or map knowledge
  map_view.py         — map rendering via dash_leaflet; build_radar_layer(),
                        build_target_markers()
  tile_server.py      — spawns local Flask tile server for MBTiles file

map_tiles/            — MBTiles file served by gui/tile_server.py
```

## HTTP API (server port 8080)

All endpoints return `application/json`.

| Method | Endpoint    | Purpose                                       |
|--------|-------------|-----------------------------------------------|
| GET    | `/health`   | Liveness: `{"status":"ok"}`                  |
| GET    | `/radar`    | Get current radar pos + terrain_msl, agl, max_range_m |
| POST   | `/radar`    | Set radar: `{lat_deg, lon_deg, agl_m}` → same as GET |
| GET    | `/elevation`| Terrain elevation: `?lat=X&lon=Y` → `{elev_m}` |
| POST   | `/query`    | Target query: `{range_m, azimuth_deg, elevation_deg, terrain_msl_m}` → `{lat_deg, lon_deg, alt_msl_m, terrain_msl_m, agl_m, horiz_range_m, relative_elev_deg}` |
| POST   | `/convert`  | Coord conversion: `{direction, ...}` — see below |

**`/convert` request body:**
```json
// LL → UTM
{"direction":"ll_to_utm","lat_deg":32.0,"lon_deg":35.0}
→ {"easting":717123.4,"northing":3544821.0,"zone":36,"hemisphere":"N"}

// UTM → LL
{"direction":"utm_to_ll","easting":717123.4,"northing":3544821.0,"zone":36,"hemisphere":"N"}
→ {"lat_deg":32.0,"lon_deg":35.0}
```

**Error codes:** 400 (bad input), 404 (radar not set), 422 (outside DEM coverage), 500 (internal)  
All errors: `{"error": "message"}`

## Key Design Decisions

**SRTM format auto-detection**: file size determines resolution.
- SRTM3: 1201×1201×2 = 2,884,802 bytes, post_spacing = 1/1200°
- SRTM1: 3601×3601×2 = 25,934,402 bytes, post_spacing = 1/3600°

**DtedTile stores its own dims** (not global constants) so mixed SRTM1/SRTM3 tiles can coexist.

**Bicubic interpolation** (Catmull-Rom kernel) over 4×4 post neighborhood — smooth sub-post elevation.

**Flat-Earth geometry** — acceptable error < 200 m at ≤50 km range.

**ElevationLUT**: pre-computed 2D float table [range_idx][az_idx], built once at startup (~9 s, ~45 MB). Per-target query is O(1) — needed for high-rate target streams.

**loadTilesAround()**: computes degree bounding box from radar pos + max_range, loads only tiles that exist in `./tiles/`. No hardcoded lat/lon.

**UTM conversion**: Snyder series (6th-order), accurate to ~1 mm within a UTM zone. Zone auto-detected from longitude; hemisphere from latitude sign.

## Server Architecture

```
main_server.cpp
  └── QueryHandler(max_range_m, tiles_dir)   ← owns DemDatabase, no HTTP
  └── RadarServer(handler, port).start()     ← registers routes, delegates to handler
        GET  /health
        GET  /radar          → handler.radar(), handler.getElevation()
        POST /radar          → handler.setRadar()
        GET  /elevation      → handler.getElevation()
        POST /query          → handler.handle(RadarQuery)
        POST /convert        → ll_to_utm() / utm_to_ll() (stateless)
```

## GUI Architecture

```
app.py  main()
  ├── tile_server.start()          → local Flask server for map tiles
  ├── RadarApiClient(server_url)   → all HTTP calls go through this
  ├── controls.layout()            → left panel (inputs, no state)
  ├── map_view.layout()            → right panel (map, no state)
  └── Dash callbacks:
        set_radar()                → POST /radar, updates radar-store
        update_radar_layer()       → reads radar-store, calls map_view.build_radar_layer()
        handle_buttons()           → POST /query, optional Open Elevation enrichment
        update_map()               → reads targets-store, calls map_view.build_target_markers()
        show_elevation()           → GET /elevation on map click
        convert_coords()           → POST /convert, updates converter output display
```

## Tile Coverage

35 SRTM3 `.hgt` files in `server/tiles/`, covering Israel (lat 28–35, lon 30–35).  
Source: viewfinderpanoramas.org (free, no login).  
Sea areas return 0 m elevation (correct — SRTM has no bathymetry).

## Git Remote

`https://github.com/david12720/radar_sea_level.git`  
Push via: `wsl bash -c "cd /mnt/c/Users/david/Downloads/radar_sea_level && git push"`

## Do Not

- Do not mention Claude in any documentation or comments
- Do not add comments unless logic is genuinely non-obvious
- Do not add docstrings to unchanged code
