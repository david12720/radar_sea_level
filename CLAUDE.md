# radar_sea_level — Claude Code Context

## What This Project Does

Computes **ground elevation (MSL)** beneath a radar target and the target's **height above ground level (AGL)**.

Input: radar position (LLA) + measurement (slant range, azimuth, elevation angle)  
Output: target lat/lon, altitude MSL, ground elevation MSL, AGL

## Build

Compiler is **WSL g++** (no Windows compiler). Always build via:
```bash
wsl bash -c "cd /mnt/c/Users/david/Downloads/radar_sea_level/server && g++ -std=c++17 -O2 -Iinclude -o radar_sea_level main.cpp src/dted_tile.cpp src/dem_database.cpp src/elevation_lut.cpp src/radar_compute.cpp"
```

Or with CMake:
```bash
wsl bash -c "cd /mnt/c/Users/david/Downloads/radar_sea_level/server && mkdir -p build && cd build && cmake .. && make"
```

Run (from `server/` so `./tiles/` resolves correctly):
```bash
wsl bash -c "cd /mnt/c/Users/david/Downloads/radar_sea_level/server && ./radar_sea_level"
```

## Project Structure

```
server/
  include/
    constants.h      — DEG2RAD, RAD2DEG, EARTH_RADIUS, SRTM1/SRTM3 dimensions
    types.h          — LLA, RadarMeasurement, TargetResult, GroundPoint
    dted_tile.h      — DtedTile (stores cols, rows, post_spacing per instance)
    dem_database.h   — DemDatabase (multi-tile manager, bicubic interpolation)
    elevation_lut.h  — LutConfig, ElevationLUT (pre-computed range/az table)
    radar_compute.h  — computeTargetSeaLevel() (two overloads)
  src/
    dted_tile.cpp    — loadSRTM (auto-detects SRTM1/SRTM3 by file size), loadDTED2
    dem_database.cpp — bicubicInterpolate, loadTile, getElevation, loadTilesAround
    elevation_lut.cpp — build(), lookup()
    radar_compute.cpp — groundPoint(), computeGeometry(), computeTargetSeaLevel()
  main.cpp           — demo: load tiles, build LUT, run test queries
  tiles/             — .hgt files (gitignored, SRTM3 1201×1201, Israel coverage)
gui/                 — Python map viewer (runs independently on port 5000)
map_tiles/           — MBTiles file served by gui/tile_server.py
```

## Key Design Decisions

**SRTM format auto-detection**: file size determines resolution.
- SRTM3: 1201×1201×2 = 2,884,802 bytes, post_spacing = 1/1200°
- SRTM1: 3601×3601×2 = 25,934,402 bytes, post_spacing = 1/3600°

**DtedTile stores its own dims** (not global constants) so mixed SRTM1/SRTM3 tiles can coexist.

**Bicubic interpolation** (Catmull-Rom kernel) over 4×4 post neighborhood — smooth sub-post elevation.

**Flat-Earth geometry** — acceptable error < 200 m at ≤50 km range.

**ElevationLUT**: pre-computed 2D float table [range_idx][az_idx], built once at startup (~9 s, ~45 MB). Per-target query is O(1) — needed for high-rate target streams.

**loadTilesAround()**: computes degree bounding box from radar pos + max_range, loads only tiles that exist in `./tiles/`. No hardcoded lat/lon.

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
