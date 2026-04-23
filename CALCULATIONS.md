# Calculation Reference

## 1. Slant-Range Decomposition

A radar measurement gives **slant range** (line-of-sight distance) and **elevation angle** above the horizon. The first step is splitting the slant range into a horizontal ground distance and a vertical height gain.

```
                              T  (target)
                             /|
              slant range   / |  vert_offset = range * sin(el)
              (range_m)    /  |
                          / el|
              radar (R)--+----+ nadir on ground
                         |<-->|
                    horiz_range = range * cos(el)
```

```cpp
horiz_range = range_m * cos(elevation_rad)   // ground-projected distance
vert_offset = range_m * sin(elevation_rad)   // signed height above radar
target_alt  = radar_alt + vert_offset
```

Elevation angle is positive above the horizon, negative below. A descending target gives a negative `vert_offset`, so target altitude can be lower than radar altitude.

---

## 2. Flat-Earth Ground Projection

Given `horiz_range` and `azimuth` (clockwise from True North), the target's ground footprint is computed using a flat-Earth model.

```
              N (0°)
              |
              |     * target footprint
              |    /|
              |   / |  Δlat
              |  /  |
              | / az|
   W -------- R ----+------- E
                  Δlon
              |
              S (180°)
```

```cpp
Δlat = horiz_range * cos(az_rad) / EARTH_RADIUS          // radians → degrees (* RAD2DEG)
Δlon = horiz_range * sin(az_rad) / (EARTH_RADIUS * cos(radar_lat_rad))
```

**Why divide longitude by `cos(lat)`?**

One degree of latitude is always ~111 km regardless of position. One degree of longitude shrinks toward the poles because meridians converge:

```
Equator  (lat=0°):  1° lon ≈ 111 km   (cos 0° = 1.0)
Israel   (lat=32°): 1° lon ≈  94 km   (cos 32° ≈ 0.85)
Poles    (lat=90°): 1° lon →   0 km   (cos 90° = 0.0)
```

Dividing by `cos(lat)` converts meters to the correct number of degrees for the current latitude.

**Error:** flat-Earth error grows with range. At 50 km range the curvature error is < 200 m — acceptable for the intended use case.

---

## 3. SRTM File Structure and Loading

### Format auto-detection by file size

`.hgt` files contain raw big-endian `int16` elevation posts (meters). File size determines resolution:

| Format | Grid      | File size        | Post spacing |
|--------|-----------|------------------|--------------|
| SRTM3  | 1201×1201 | 2,884,802 bytes  | 1/1200°≈90 m |
| SRTM1  | 3601×3601 | 25,934,402 bytes | 1/3600°≈30 m |

### Row order and internal flip

The file stores rows **North → South** (row 0 = northernmost latitude). Internally, posts are indexed **South → North** so that increasing `row` index corresponds to increasing latitude — consistent with geographic convention.

```
File row 0  (North edge) → internal row = rows-1
File row 1               → internal row = rows-2
...
File row rows-1 (South)  → internal row = 0
```

```cpp
south_row = rows - 1 - r;          // r = file row index
elevation[col * rows + south_row]  // column-major layout
```

### Big-endian to host conversion

```cpp
int16_t value = (byte[0] << 8) | byte[1];   // manual big-endian decode
```

Void posts (`-32768`) are treated as `0.0` (sea level).

---

## 4. Geographic Coordinates to Post Index

Given a query point `(lat, lon)`, the tile that covers it has its SW corner at `(floor(lat), floor(lon))`.

```
tile SW corner = (tile_lat, tile_lon) = (floor(lat), floor(lon))

frac_col = (lon - tile_lon) / post_spacing
frac_row = (lat - tile_lat) / post_spacing
```

Example — SRTM3 tile at (32°N, 35°E), querying (32.5°N, 35.5°E):

```
post_spacing = 1/1200 ≈ 0.000833°

frac_col = (35.5 - 35) / (1/1200) = 0.5 * 1200 = 600
frac_row = (32.5 - 32) / (1/1200) = 0.5 * 1200 = 600
```

Both equal 600, which is exactly the center of the 1201×1201 tile.

---

## 5. Bicubic Interpolation (Catmull-Rom)

**Why not bilinear?** Bilinear uses only the 4 nearest posts and produces visible flat triangular patches on smooth terrain. Catmull-Rom uses a **4×4 neighborhood** and is C¹-continuous — slopes match across post boundaries.

### 4×4 Sampling Neighborhood

```
columns:  c0-1   c0    c0+1  c0+2
           |      |      |     |
row r0-1  [ ]    [ ]    [ ]   [ ]
row r0    [ ]    [●]----[●]   [ ]   ← query falls in this cell
row r0+1  [ ]    [●]----[●]   [ ]      dc = frac_col - c0  (0 ≤ dc < 1)
row r0+2  [ ]    [ ]    [ ]   [ ]      dr = frac_row - r0  (0 ≤ dr < 1)
```

`c0 = floor(frac_col)`, `r0 = floor(frac_row)`. The query is at fractional offset `(dc, dr)` inside the cell formed by posts `(c0, r0)` to `(c0+1, r0+1)`.

### Catmull-Rom Weight Function

Each of the 4 posts in each axis gets a weight based on its distance `t` from the query point:

```
         ┌  1.5|t|³ - 2.5|t|² + 1        if |t| < 1   (inner: smoothly blends 2 nearest)
w(t)  =  ┤
         └ -0.5|t|³ + 2.5|t|² - 4|t| + 2  if 1 ≤ |t| < 2  (outer: tapers flanking posts)
           0                                otherwise
```

Weight profile across the 4 posts (example at dc = 0.3):

```
post index:   c0-1      c0      c0+1    c0+2
distance t:   -1.3     -0.3     0.7     1.7
weight:        small   large   medium   tiny
```

### Separable 2D Sum

Weights are separable — the 2D value is a sum of all 16 products:

```
elevation = Σ  w(ci - dc) * w(ri - dr) * post[c0+ci][r0+ri]
           ci ∈ {-1,0,1,2}
           ri ∈ {-1,0,1,2}
```

**Boundary clamping:** posts outside the tile extent are clamped to the nearest edge post, preventing reads out of the array and avoiding artifacts for near-boundary queries.

---

## 6. Tile Bounding Box (loadTilesAround)

To know which tiles to load, the radar coverage circle is enclosed in a degree bounding box. The box is **not square in degrees** because the longitude span of a meter changes with latitude.

```
lat_span = max_range_m / EARTH_RADIUS * RAD2DEG
lon_span = max_range_m / (EARTH_RADIUS * cos(radar_lat_rad)) * RAD2DEG

Bounding box (1°×1° tile SW corners):
  lat: floor(radar_lat - lat_span)  →  floor(radar_lat + lat_span)
  lon: floor(radar_lon - lon_span)  →  floor(radar_lon + lon_span)
```

Example — radar at 32°N, max_range = 60 km:

```
lat_span = 60,000 / 6,371,000 * 57.3 ≈ 0.54°
lon_span = 0.54° / cos(32°) ≈ 0.64°

Bounding box: lat [31, 32], lon [34, 36]  → up to 2×3 = 6 tiles checked
```

`lon_span > lat_span` at northern latitudes because degrees of longitude are shorter in meters there — the bounding box is wider in longitude to compensate.

Only tiles whose `.hgt` file actually exists on disk are loaded.

---

## 7. Elevation LUT Build

The LUT pre-computes ground elevation for every `(range, azimuth)` bin once at startup, so per-target queries during operation are O(1) array lookups instead of DEM interpolations.

### Grid Dimensions

```
num_ranges   = ceil(max_range_m / range_step_m) + 1
num_azimuths = round(360 / az_step_deg)
```

Example: 60 km range @ 15 m step, 0.1° az step → 4001 × 3600 = 14.4 M cells × 4 bytes = **57.6 MB**.

> **Note — SRTM-3 resolution vs step sizes:** SRTM-3 posts are spaced ~90 m apart. A 15 m range step creates ~6 LUT bins per real data interval; bicubic interpolation keeps them smooth but adds no genuine terrain detail beyond ~90 m. Matching the range step to ~90 m would reduce memory to ~10 MB and build time by 6×, with negligible elevation error (terrain slopes are gradual). The 0.1° azimuth step ≈ 105 m arc at 60 km — already well matched to SRTM-3 spacing.

### Build Loop

```
for ri in [0, num_ranges):
    horiz_range = ri * range_step_m
    for ai in [0, num_azimuths):
        azimuth = ai * az_step_deg
        gp      = groundPoint(radar, horiz_range, azimuth)   // flat-Earth projection
        if tile exists at gp:
            table[ri][ai] = DEM.getElevation(gp)             // bicubic interpolation
        else:
            table[ri][ai] = 0.0                              // sea / outside coverage
```

`float32` per cell is intentional — 4 bytes keeps the table cache-friendly and 1 m precision is sufficient for ground elevation.

---

## 8. LUT Lookup

```
ri = round(horiz_range_m / range_step_m)     clamped to [0, num_ranges - 1]
ai = round(azimuth_deg   / az_step_deg) mod num_azimuths

ground_elevation = table[ri][ai]
```

**Round, not truncate:** rounding to the nearest bin halves the quantization error compared to floor.

**Circular wrap:** `fmod(azimuth, 360)` + sign fix normalizes to [0°, 360°), then `% num_azimuths` maps 360° back to 0 without a gap.

---

## 9. Relative Elevation Angle

After AGL is known, a second angle is computed: **how many degrees does the beam sit above the terrain line at the target's location?**

This is distinct from the beam elevation angle, which is measured from the radar's horizon. The terrain at the target may be higher or lower than the radar, so the terrain line of sight has its own angle.

### Step 1 — Elevation angle to the ground point

```
elev_to_ground = atan2(ground_elevation_MSL - radar_alt_MSL, horiz_range)
```

```
               radar (R)
              /
             / elev_to_ground (negative here — ground is below radar)
            /
           G  (ground point at target lat/lon, elevation = ground_elevation_MSL)
```

`vert = ground_elevation_MSL - radar_alt_MSL` is negative when the terrain is below the radar (common: radar is elevated, terrain slopes down). `atan2` gives the signed angle naturally.

### Step 2 — Relative elevation

```
relative_elevation_deg = beam_elevation_deg - elev_to_ground_deg
```

```
                  T  (target)
                 /
beam_elevation  /
               /  ← beam
              R
               \  ← terrain line of sight (elev_to_ground)
                \
                 G  (ground beneath target)

relative_elevation = angle between the two lines
```

| Value | Meaning |
|-------|---------|
| `> 0` | Beam is above the terrain line — target is airborne above terrain |
| `= 0` | Beam grazes the terrain surface |
| `< 0` | Terrain is higher than the beam — ground masks the target |

**Key difference from AGL:** AGL is a vertical distance in meters. Relative elevation is an angle in degrees — it captures how well the radar can "see" the target above the terrain, accounting for both target height and terrain slope.

---

## 10. Full Pipeline Summary

```
Inputs:  radar LLA  +  RadarMeasurement (range, azimuth, elevation)

                           ┌──────────────────────────────┐
  range, elevation_deg     │  Slant-range decomposition   │
  ─────────────────────►   │  horiz = range * cos(el)     │
                           │  vert  = range * sin(el)     │
                           └───────────────┬──────────────┘
                                           │ horiz_range, vert_offset
                           ┌───────────────▼──────────────┐
  horiz_range, azimuth     │  Flat-Earth ground projection │
  ─────────────────────►   │  Δlat, Δlon (cos lat factor)  │
                           └───────────────┬──────────────┘
                                           │ target lat/lon
                           ┌───────────────▼──────────────┐
                           │  DEM or LUT elevation lookup  │
                           │  tile → frac index → bicubic  │
                           └───────────────┬──────────────┘
                                           │ ground_elevation_MSL
                           ┌───────────────▼──────────────┐
                           │  AGL = target_alt - ground    │
                           └───────────────┬──────────────┘
                                           │ ground_elevation_MSL + horiz_range
                           ┌───────────────▼──────────────┐
                           │  Relative elevation angle     │
                           │  atan2(Δalt, horiz) → beam-  │
                           │  terrain angular separation   │
                           └──────────────────────────────┘

Outputs: target lat/lon/alt MSL, ground_elevation MSL, AGL, relative_elevation_deg
```

`AGL > 0`: target is above terrain.  
`AGL < 0`: target is geometrically below terrain — multipath or masked return.  
`relative_elevation_deg > 0`: beam clears the terrain — target is visible.  
`relative_elevation_deg < 0`: terrain masks the beam — target is shadowed.
