# Radar Sea Level

Computes the ground elevation (terrain height MSL) beneath a radar target and derives the target's height above ground level (AGL).

## How It Works

Given a radar position and a measurement (slant range, azimuth, elevation angle):

1. Decompose slant range into horizontal range and vertical offset
2. Project horizontal range + azimuth onto the Earth surface (flat-Earth model) to get target lat/lon
3. Query the DEM for terrain height at that lat/lon
4. AGL = target altitude MSL − ground elevation MSL

For real-time use with large target volumes, a pre-computed lookup table (LUT) maps every (range, azimuth) cell to its ground elevation — built once at startup, O(1) per query at runtime.

## Project Structure

```
radar_sea_level/
├── include/
│   ├── constants.h       # Math constants, SRTM resolution parameters
│   ├── types.h           # LLA, RadarMeasurement, TargetResult, GroundPoint
│   ├── dted_tile.h       # Single 1°×1° DEM tile
│   ├── dem_database.h    # Multi-tile manager with bicubic interpolation
│   ├── elevation_lut.h   # Pre-computed range/azimuth elevation table
│   └── radar_compute.h   # Core geometry and AGL computation
├── src/
│   ├── dted_tile.cpp
│   ├── dem_database.cpp
│   ├── elevation_lut.cpp
│   └── radar_compute.cpp
├── main.cpp              # Entry point and demo
├── tiles/                # DEM tile files (.hgt) — not committed
└── CMakeLists.txt
```

## DEM Tile Formats Supported

| Format | Extension | Resolution | Posts/tile |
|--------|-----------|------------|------------|
| SRTM Level-1 | `.hgt` | ~30 m (1 arc-second) | 3601 × 3601 |
| SRTM Level-3 | `.hgt` | ~90 m (3 arc-second) | 1201 × 1201 |
| DTED Level-2 | `.dt2` | ~30 m (1 arc-second) | 3601 × 3601 |

Resolution is auto-detected from file size. Free SRTM3 tiles (no login required): [viewfinderpanoramas.org/dem3.html](https://viewfinderpanoramas.org/dem3.html)

## Build

Requires a C++17 compiler and CMake 3.15+.

```bash
mkdir build && cd build
cmake ..
make
```

Or compile directly:

```bash
g++ -std=c++17 -O2 -Iinclude -o radar_sea_level \
    main.cpp src/dted_tile.cpp src/dem_database.cpp \
    src/elevation_lut.cpp src/radar_compute.cpp
```

## Usage

### 1. Place tile files in `tiles/`

Tiles must follow the standard SRTM naming convention: `N31E034.hgt`, `N32E035.hgt`, etc.

### 2. Configure, load tiles and build the LUT

```cpp
LutConfig cfg;
cfg.max_range_m  = 50000.0; // 50 km coverage radius
cfg.range_step_m = 15.0;    // 15 m range resolution
cfg.az_step_deg  = 0.1;     // 0.1° azimuth resolution

// Automatically loads only the tiles that fall within max_range_m of the radar
DemDatabase dem;
dem.loadTilesAround(radar, cfg.max_range_m, "./tiles/", DemDatabase::Format::SRTM);

ElevationLUT lut;
lut.build(radar, dem, cfg);
```

### 3. Query per target (real-time)

```cpp
LLA radar { 32.08, 34.76, 10.0 }; // lat, lon, altitude MSL (m)
RadarMeasurement meas { 8000.0, 45.0, -0.5 }; // range (m), azimuth (deg), elevation (deg)

TargetResult res = computeTargetSeaLevel(radar, meas, lut);

// res.position.alt_m       — target altitude MSL
// res.ground_elevation_m   — terrain height at target lat/lon
// res.target_height_agl_m  — target height above ground
```

## Output Fields

| Field | Description |
|-------|-------------|
| `position.lat_deg / lon_deg` | Target geographic position |
| `position.alt_m` | Target altitude above mean sea level |
| `ground_elevation_m` | Terrain height MSL at target location (from DEM) |
| `target_height_agl_m` | Target altitude minus ground elevation |
| `horizontal_range_m` | Ground-projected range from radar |
