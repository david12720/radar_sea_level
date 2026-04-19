#pragma once
#include <cmath>

inline constexpr double DEG2RAD      = M_PI / 180.0;
inline constexpr double RAD2DEG      = 180.0 / M_PI;
inline constexpr double EARTH_RADIUS = 6'371'000.0; // meters

// SRTM HGT Level-1 (1 arc-second, ~30 m)
inline constexpr int    SRTM1_COLS         = 3601;
inline constexpr int    SRTM1_ROWS         = 3601;
inline constexpr double SRTM1_POST_SPACING = 1.0 / 3600.0;

// SRTM HGT Level-3 (3 arc-second, ~90 m) — viewfinderpanoramas.org
inline constexpr int    SRTM3_COLS         = 1201;
inline constexpr int    SRTM3_ROWS         = 1201;
inline constexpr double SRTM3_POST_SPACING = 1.0 / 1200.0;
