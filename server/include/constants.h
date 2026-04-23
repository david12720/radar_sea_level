#pragma once
#include <cmath>

// M_PI is not guaranteed by the standard; define our own.
inline constexpr double PI           = 3.14159265358979323846;
inline constexpr double DEG2RAD      = PI / 180.0;
inline constexpr double RAD2DEG      = 180.0 / PI;
inline constexpr double EARTH_RADIUS = 6'371'000.0; // meters

// SRTM HGT Level-1 (1 arc-second, ~30 m)
inline constexpr int    SRTM1_COLS         = 3601;
inline constexpr int    SRTM1_ROWS         = 3601;
inline constexpr double SRTM1_POST_SPACING = 1.0 / 3600.0;

// SRTM HGT Level-3 (3 arc-second, ~90 m) — viewfinderpanoramas.org
inline constexpr int    SRTM3_COLS         = 1201;
inline constexpr int    SRTM3_ROWS         = 1201;
inline constexpr double SRTM3_POST_SPACING = 1.0 / 1200.0;

// Maximum bounds for static allocations
inline constexpr int MAX_LUT_RANGES   = 4000; // e.g. 60km @ 15m step
inline constexpr int MAX_LUT_AZIMUTHS = 3600; // e.g. 360 deg @ 0.1 deg step
inline constexpr int MAX_DEM_TILES    = 9;    // 3x3 grid around radar
