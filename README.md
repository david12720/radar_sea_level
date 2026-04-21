# Radar Sea Level

Computes the ground elevation (terrain height MSL) beneath a radar target and derives the target's height above ground level (AGL).

## How It Works

Given a radar position and a measurement (slant range, azimuth, elevation angle):

1. Decompose slant range into horizontal range and vertical offset
2. Project horizontal range + azimuth onto the Earth surface (flat-Earth model) to get target lat/lon
3. Query the DEM for terrain height at that lat/lon
4. AGL = target altitude MSL − ground elevation MSL

For real-time use with large target volumes, a pre-computed lookup table (LUT) maps every (range, azimuth) cell to its ground elevation — built once at startup, O(1) per query at runtime.

## Developer Setup (Windows 10)

### C++ dependencies

All C++ dependencies are **vendored** — no separate installs required:

| Library | Location | Purpose |
|---------|----------|---------|
| [cpp-httplib](https://github.com/yhirose/cpp-httplib) | `include/vendor/httplib.h` | HTTP server (header-only) |
| [nlohmann/json](https://github.com/nlohmann/json) | `include/vendor/json.hpp` | JSON serialization (header-only) |

**Compiler — choose one:**

- **Visual Studio 2022** (recommended): install the **"Desktop development with C++"** workload. CMake is not required — open `radar_sea_level.sln` directly.
- **WSL g++**: install via `sudo apt install g++` inside WSL.

### Python dependencies

- **Python 3.11+** — [python.org](https://www.python.org/downloads/)
- Packages: `pip install -r gui/requirements.txt`

| Package | Purpose |
|---------|---------|
| dash | Web UI framework |
| dash-leaflet | Interactive map component |
| flask | Embedded tile server |
| requests | HTTP client for C++ server |

### Data files (not in repo)

| What | Where to put it | How to get it |
|------|----------------|---------------|
| DEM elevation tiles (`.hgt`) | `tiles/` | [viewfinderpanoramas.org/dem3.html](https://viewfinderpanoramas.org/dem3.html) — free, no login |
| Offline map tiles (`.mbtiles`) | `map_tiles/` | Run `pip install download-tiles` then `python map_tiles/download_tiles.py` (~106 MB) |

### Ports used

| Port | Component | Configurable |
|------|-----------|-------------|
| 8080 | C++ radar server | `--port` arg |
| 8050 | Python GUI (Dash) | hardcoded in `app.py` |
| 8081 | Tile server (Flask) | auto-assigned |

All three must be free before starting the system.

---

## Quick Start

### One-time setup

**Step 1 — Build the C++ server**

*Option A — Visual Studio 2022:*
Open `radar_sea_level.sln`, select **Release x64**, press **Build**. Binary appears at `x64\Release\radar_server.exe`.

*Option B — WSL g++:*
```bash
cd /mnt/c/Users/<you>/radar_sea_level
g++ -std=c++17 -O2 -Iinclude -Iapp -o radar_server \
    main_server.cpp app/query_handler.cpp app/radar_server.cpp \
    src/dted_tile.cpp src/dem_database.cpp \
    src/elevation_lut.cpp src/radar_compute.cpp -lpthread
```

**Step 2 — Download DEM tiles** (terrain elevation data)

Download SRTM3 `.hgt` files for your area from [viewfinderpanoramas.org](https://viewfinderpanoramas.org/dem3.html) and place them in `tiles/`.
Tile naming example: `N32E034.hgt`.

**Step 3 — Download offline map tiles** (one-time, ~106 MB):
```bash
pip install download-tiles
python map_tiles/download_tiles.py
```

**Step 4 — Install Python dependencies**:
```bash
pip install -r gui/requirements.txt
```

### Running the system

**Terminal 1 — Start the C++ server**

Windows (from repo root):
```cmd
x64\Release\radar_server.exe --lat 32.08 --lon 34.76 --alt 10 --max-range 50000
```
WSL:
```bash
./radar_server --lat 32.08 --lon 34.76 --alt 10 --max-range 50000
```
Wait for `[server] Listening on port 8080` before continuing.

`--alt` sets radar altitude MSL in meters. `--max-range` sets the detection radius in meters. These values are read by the GUI at startup and shown when you click the radar marker on the map.

**Terminal 2 — Start the GUI** (Windows):
```bash
python gui/app.py
```
Open **http://localhost:8050** in your browser.

### Using the GUI

1. **Set your measurement** using the three sliders on the left panel:
   - **Range** — slant range to the target in meters
   - **Azimuth** — direction from North (0° = North, 90° = East, 180° = South, 270° = West)
   - **Elevation** — beam elevation angle (0° = horizontal, positive = upward)

2. **Click "Add Target"** — the target appears as a colored pin on the map

3. **Click a pin** to see the popup with full details:
   - Lat / Lon — geographic position
   - Alt MSL — target altitude above mean sea level
   - Ground — terrain height at that location (from DEM)
   - AGL — target height above the ground
   - Range / Az / El — the original measurement

4. **Pin colors** indicate how high the target is above ground:

   | Color | AGL | Meaning |
   |-------|-----|---------|
   | 🔴 Red | < 50 m | Near ground / sea-skimmer |
   | 🟠 Orange | 50–300 m | Low altitude |
   | 🟡 Yellow | 300–1000 m | Medium altitude |
   | 🟢 Green | > 1000 m | High altitude |

5. **Click anywhere on the map** to see the terrain elevation at that point — lat, lon, and elevation MSL appear in a bar at the bottom of the map.

6. **Click "Clear All"** to reset the map and start a new session.

The blue circle on the map shows the radar's maximum configured range. The blue marker at the center is the radar position.

---

## Project Structure

```
radar_sea_level/
├── include/
│   ├── constants.h           # Math constants, SRTM resolution parameters
│   ├── types.h               # LLA, RadarMeasurement, TargetResult, GroundPoint
│   ├── dted_tile.h           # Single 1°×1° DEM tile
│   ├── dem_database.h        # Multi-tile manager with bicubic interpolation
│   ├── elevation_lut.h       # Pre-computed range/azimuth elevation table
│   ├── radar_compute.h       # Core geometry and AGL computation
│   └── vendor/               # Header-only dependencies (httplib, nlohmann/json)
├── src/
│   ├── dted_tile.cpp
│   ├── dem_database.cpp
│   ├── elevation_lut.cpp
│   └── radar_compute.cpp
├── app/
│   ├── query_handler.h/.cpp  # Pure domain logic: RadarQuery → TargetResult
│   └── radar_server.h/.cpp   # HTTP transport layer (cpp-httplib)
├── gui/
│   ├── app.py                # Dash application entry point
│   ├── api_client.py         # HTTP client for the C++ server
│   ├── tile_server.py        # Local MBTiles tile server (offline map)
│   ├── controls.py           # Input panel layout
│   ├── map_view.py           # Leaflet map with target markers
│   └── requirements.txt
├── tests/
│   └── test_query_handler.cpp
├── main.cpp                  # CLI demo (direct LUT query)
├── main_server.cpp           # HTTP server entry point
├── map_tiles/                # Offline map tiles (.mbtiles) — not committed
├── tiles/                    # DEM tile files (.hgt) — not committed
└── CMakeLists.txt
```

## DEM Tile Formats Supported

| Format | Extension | Resolution | Posts/tile |
|--------|-----------|------------|------------|
| SRTM Level-1 | `.hgt` | ~30 m (1 arc-second) | 3601 × 3601 |
| SRTM Level-3 | `.hgt` | ~90 m (3 arc-second) | 1201 × 1201 |
| DTED Level-2 | `.dt2` | ~30 m (1 arc-second) | 3601 × 3601 |

Resolution is auto-detected from file size. Free SRTM3 tiles (no login required): [viewfinderpanoramas.org/dem3.html](https://viewfinderpanoramas.org/dem3.html)

## Build (C++)

Requires a C++17 compiler and CMake 3.15+. On Windows, build via WSL.

```bash
# Demo binary
g++ -std=c++17 -O2 -Iinclude -o radar_sea_level \
    main.cpp src/dted_tile.cpp src/dem_database.cpp \
    src/elevation_lut.cpp src/radar_compute.cpp

# HTTP server binary
g++ -std=c++17 -O2 -Iinclude -Iapp -o radar_server \
    main_server.cpp app/query_handler.cpp app/radar_server.cpp \
    src/dted_tile.cpp src/dem_database.cpp \
    src/elevation_lut.cpp src/radar_compute.cpp -lpthread
```

## Usage — HTTP Server + GUI

### 1. Prepare offline map tiles (one-time)

```bash
pip install download-tiles
python map_tiles/download_tiles.py   # downloads ~106 MB for Israel region
```

### 2. Prepare DEM tiles

Place `.hgt` files in `tiles/` following the SRTM naming convention: `N31E034.hgt`, etc.

### 3. Start the C++ server

```bash
./radar_server [--lat 32.08] [--lon 34.76] [--alt 10] \
               [--port 8080] [--tiles ./tiles/] [--max-range 50000]
```

All arguments are optional — defaults to Tel Aviv coast, port 8080, 50 km range.

### 4. Start the GUI

```bash
pip install -r gui/requirements.txt
python gui/app.py
```

Open `http://localhost:8050` in a browser. Set range, azimuth, and elevation, click **Add Target** to plot measurements on the map. Targets are color-coded by AGL:

| Color | AGL |
|-------|-----|
| 🔴 Red | < 50 m |
| 🟠 Orange | 50–300 m |
| 🟡 Yellow | 300–1000 m |
| 🟢 Green | > 1000 m |

The map runs fully offline using locally cached tiles.

## Usage — Library / Direct Query

### Configure, load tiles and build the LUT

```cpp
LutConfig cfg;
cfg.max_range_m  = 50000.0; // 50 km coverage radius
cfg.range_step_m = 15.0;    // 15 m range resolution
cfg.az_step_deg  = 0.1;     // 0.1° azimuth resolution

DemDatabase dem;
dem.loadTilesAround(radar, cfg.max_range_m, "./tiles/", DemDatabase::Format::SRTM);

ElevationLUT lut;
lut.build(radar, dem, cfg);
```

### Query per target (real-time)

```cpp
LLA radar { 32.08, 34.76, 10.0 }; // lat, lon, altitude MSL (m)
RadarMeasurement meas { 8000.0, 45.0, -0.5 }; // range (m), azimuth (deg), elevation (deg)

TargetResult res = computeTargetSeaLevel(radar, meas, lut);

// res.position.alt_m       — target altitude MSL
// res.ground_elevation_m   — terrain height at target lat/lon
// res.target_height_agl_m  — target height above ground
```

### HTTP API

```
POST /query
Content-Type: application/json
{ "range_m": 8000, "azimuth_deg": 45.0, "elevation_deg": -0.5 }

→ 200 { "lat_deg": ..., "lon_deg": ..., "alt_msl_m": ...,
         "ground_elev_m": ..., "agl_m": ..., "horiz_range_m": ... }
→ 400 { "error": "validation failed: ..." }
→ 422 { "error": "no DEM coverage at target location" }

GET /health → { "status": "ok" }
GET /radar  → { "lat_deg": ..., "lon_deg": ..., "alt_m": ..., "max_range_m": ... }
GET /elevation?lat=32.08&lon=34.76 → { "elev_m": ... }   (0.0 if outside tile coverage)
```

## Output Fields

| Field | Description |
|-------|-------------|
| `position.lat_deg / lon_deg` | Target geographic position |
| `position.alt_m` | Target altitude above mean sea level |
| `ground_elevation_m` | Terrain height MSL at target location (from DEM) |
| `target_height_agl_m` | Target altitude minus ground elevation |
| `horizontal_range_m` | Ground-projected range from radar |
