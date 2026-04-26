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
| Offline map tiles (`.mbtiles`) | `map_tiles/` | Run `python map_tiles/download_tiles.py` (~106 MB for Israel) |

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
    src/elevation_lut.cpp src/radar_compute.cpp src/relative_angle.cpp -lpthread
```

**Step 2 — Download DEM tiles** (terrain elevation data)

Download SRTM3 `.hgt` files for your area from [viewfinderpanoramas.org](https://viewfinderpanoramas.org/dem3.html) and place them in `tiles/`.
Tile naming example: `N32E034.hgt`.

**Step 3 — Download offline map tiles** (one-time, ~106 MB for Israel):
```bash
python map_tiles/download_tiles.py

# Other region or zoom level:
python map_tiles/download_tiles.py --bbox -6.0 49.5 2.5 61.0 --max-zoom 14
```
`--bbox west south east north` — bounding box in decimal degrees. Default: Israel.
`--max-zoom` — highest zoom level to download (default: 16). Re-running resumes automatically.

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

`--alt` sets the radar height above local ground in meters (AGL). The server resolves altitude MSL automatically from the DEM — you do not need to know your elevation above sea level. `--max-range` sets the detection radius in meters.

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
    src/elevation_lut.cpp src/radar_compute.cpp src/relative_angle.cpp -lpthread

# LUT TCP export server
g++ -std=c++17 -O2 -Iinclude -Iapp -o lut_server \
    main_lut_server.cpp app/lut_exporter.cpp app/lut_tcp_server.cpp \
    src/dted_tile.cpp src/dem_database.cpp \
    src/elevation_lut.cpp src/radar_compute.cpp src/relative_angle.cpp -lpthread
```

## Usage — HTTP Server + GUI

### 1. Prepare offline map tiles (one-time)

```bash
python map_tiles/download_tiles.py                          # Israel, z0-16 (default)
python map_tiles/download_tiles.py --max-zoom 13            # lower zoom, fewer tiles
python map_tiles/download_tiles.py --bbox W S E N          # custom region
```
Re-running the script resumes from where it left off — already-downloaded tiles are skipped.

### 2. Prepare DEM tiles

Place `.hgt` files in `tiles/` following the SRTM naming convention: `N31E034.hgt`, etc.

### 3. Start the C++ server

```bash
./radar_server [--lat 32.08] [--lon 34.76] [--alt 0] \
               [--port 8080] [--tiles ./tiles/] [--max-range 50000]
```

All arguments are optional — defaults to Tel Aviv coast, port 8080, 50 km range.

`--alt` is the radar height **above local ground** in meters (AGL). The server automatically looks up the terrain elevation at the radar's lat/lon from the DEM and computes radar altitude MSL = terrain + `--alt`. You do not need to know your altitude above sea level.

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

## Usage — LUT TCP Export Server

Exports a full ground-elevation LUT for an arbitrary radar position as a raw binary `int32[az][range]` array over TCP.

### Start the server

```bash
./lut_server [--tiles ./tiles/] [--port 9000]
```

### Wire protocol (binary, little-endian)

**Client → server** (fixed 33-byte header):

| Field | Type | Size |
|-------|------|------|
| `lat_deg` | double | 8 |
| `lon_deg` | double | 8 |
| `alt_m` | double | 8 |
| `max_range_m` | double | 8 |
| `mode` | uint8 | 1 |

If `mode == 1` (save to file on server), append:

| Field | Type | Size |
|-------|------|------|
| `filename_len` | uint16 | 2 |
| `filename` | char[] | filename_len |

**Server → client**:

- `mode 0` (return data): `uint32 az_count`, `uint32 range_count`, then `int32[az_count * range_count]`
- `mode 1` (save to file): `uint8 status` (0 = ok, 1 = error)

### Array layout

```
int32[az_count][range_count]   stored row-major, azimuth outer
```

- `az_count` = 3600 (0.1° steps, index 0 = 0°, index 12 = 1.2°)
- `range_count` = `ceil(max_range_m / 15) + 1` (15 m steps, index 100 = 1500 m)
- Value: ground elevation MSL in meters, rounded to nearest integer

To read element at azimuth 1.2° (index 12), range 1500 m (index 100):
```c
int32_t elev = cells[12 * range_count + 100];
```

File saved with `mode 1` is the same raw block with no header.

---

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
{ "range_m": 8000, "azimuth_deg": 45.0, "elevation_deg": -0.5,
  "earth_model": "flat" | "sphere" | "k43"   (optional; default "flat") }

→ 200 { "lat_deg": ..., "lon_deg": ..., "alt_msl_m": ...,
         "ground_elev_m": ..., "agl_m": ..., "horiz_range_m": ...,
         "relative_elev_deg": ..., "earth_model": "flat" }
→ 400 { "error": "validation failed: ..." }
→ 422 { "error": "no DEM coverage at target location" }

GET /health → { "status": "ok" }
GET /radar  → { "lat_deg": ..., "lon_deg": ..., "alt_m": ..., "max_range_m": ... }
GET /elevation?lat=32.08&lon=34.76 → { "elev_m": ... }   (0.0 if outside tile coverage)
```

## Validating Elevation Accuracy

The system reads terrain elevation from SRTM DEM data. You can cross-check any point against an independent online source:

**Open-Elevation API** (also SRTM-based, free, no login):
```
https://api.open-elevation.com/api/v1/lookup?locations=<lat>,<lon>
```
Example — Har Hiriya, Tel Aviv area:
```
https://api.open-elevation.com/api/v1/lookup?locations=32.02896,34.82118
```
Returns JSON: `{"results": [{"latitude": 32.02896, "longitude": 34.82118, "elevation": 68}]}`

### Expected accuracy

A difference of **2–5 m** between this system and Open-Elevation is normal and expected. Both use SRTM data but may differ due to:
- Interpolation method (this system uses bicubic Catmull-Rom; Open-Elevation uses nearest-post)
- Exact sample point within the tile

A difference larger than ~10 m usually means the queried coordinate is near a tile boundary or a steep terrain feature where SRTM post spacing (90 m for SRTM3) limits precision.

### Note on landmark labels vs. DEM elevation

Map labels (e.g. "הר חירייה 75m") show the commonly cited or engineered height of a landmark — not necessarily what the SRTM satellite measured. Har Hiriya, for example, is a man-made landfill hill; its SRTM-measured terrain height (~66–68 m) is lower than the engineering design figure (75 m) cited on the map. This is expected and does not indicate an error in the system.

---

## Validating Target Position Accuracy

The `/query` endpoint supports three earth models (chosen per request via the `earth_model` field, or from the GUI dropdown):

| Model    | Formula                  | When to use                                              |
|----------|--------------------------|----------------------------------------------------------|
| `flat`   | Tangent-plane trig       | Short ranges (< ~20 km), backward-compatible default     |
| `sphere` | Great-circle + true sphere (R = 6371 km) | Long ranges, no atmospheric correction |
| `k43`    | Great-circle + 4/3 Earth radius (R ≈ 8495 km) | Long ranges *with* standard atmospheric refraction — matches most radar-engineering textbooks |

**Cross-check the great-circle destination math** against the reference implementation at [movable-type.co.uk/scripts/latlong.html](https://www.movable-type.co.uk/scripts/latlong.html):

1. On the site, scroll to the **"Destination point given distance and bearing from start point"** section.
2. Enter:
   - **Start point:** your radar lat/lon
   - **Bearing:** the target azimuth
   - **Distance:** the *ground-arc* distance (for elevation = 0°, this equals the radar range; otherwise it's `range × cos(elevation)` for flat earth, or the `horiz_range_m` returned by `sphere` / `k43`)
3. Query `/query` with the same radar, azimuth and range, and `earth_model=sphere`.
4. Compare the returned `lat_deg` / `lon_deg` to the movable-type result — they should agree to < 1 cm for distances under ~100 km.

Expected differences **between earth models** at 50 km, elevation 0°:

| Error source           | flat         | sphere       | k43          |
|------------------------|--------------|--------------|--------------|
| Target altitude (m)    | ~0           | ~196 below   | ~147 below   |
| Ground arc length (m)  | 50 000       | 49 998       | 49 998       |
| Lat/lon drift (m)      | 0            | ~2           | ~2           |

These are expected, not bugs. Use `k43` if you care about AGL accuracy at long range; use `flat` for fast short-range work.

---

## Output Fields

| Field | Description |
|-------|-------------|
| `position.lat_deg / lon_deg` | Target geographic position |
| `position.alt_m` | Target altitude above mean sea level |
| `ground_elevation_m` | Terrain height MSL at target location (from DEM) |
| `target_height_agl_m` | Target altitude minus ground elevation |
| `horizontal_range_m` | Ground-projected range from radar |
