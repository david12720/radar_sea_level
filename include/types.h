#pragma once

// Geodetic position: latitude, longitude (degrees), altitude MSL (meters).
struct LLA {
    double lat_deg;
    double lon_deg;
    double alt_m;
};

// Raw radar measurement for a single target.
struct RadarMeasurement {
    double range_m;        // Slant range (line-of-sight distance)
    double azimuth_deg;    // True North clockwise, [0, 360)
    double elevation_deg;  // Positive up from horizon, [-90, 90]
};

// Full resolved result for a target query.
struct TargetResult {
    LLA    position;               // Target lat/lon/alt MSL
    double ground_elevation_m;     // Terrain MSL directly beneath target
    double target_height_agl_m;    // position.alt_m - ground_elevation_m
    double horizontal_range_m;     // Ground-projected range from radar
    double vertical_offset_m;      // range * sin(elevation) — signed height gain
    // Angle between the beam and the radar-to-target-ground line of sight.
    // Positive: beam is above terrain. Negative: terrain masks the target.
    double relative_elevation_deg;
};

// Flat-Earth ground projection result (intermediate, no elevation).
struct GroundPoint {
    double lat_deg;
    double lon_deg;
};
