#pragma once

/**
 * Fundamental Radar System Data Types.
 * Carry state across the core logic, HTTP server, and GUI.
 */

/** Geodetic position (WGS84). */
struct LLA {
    double lat_deg;
    double lon_deg;
    double alt_m;
};

/** Raw radar measurement for a single target. */
struct RadarMeasurement {
    double range_m;        // Slant range (line-of-sight distance)
    double azimuth_deg;    // True North clockwise, [0, 360)
    double elevation_deg;  // Positive up from horizon, [-10, 45]
};

/** Full resolved target state. */
struct TargetResult {
    LLA    position;               // Target lat/lon/alt MSL
    double ground_elevation_m;     // Terrain MSL directly beneath target
    double target_height_agl_m;    // Above Ground Level altitude
    double horizontal_range_m;     // Ground-projected range from radar
    double vertical_offset_m;      // vertical height gain from radar antenna
    // Angle between the beam and the radar-to-target-ground line of sight.
    // Positive: beam is above terrain. Negative: terrain masks the target.
    double relative_elevation_deg;
};

/** Geographic point without altitude. */
struct GroundPoint {
    double lat_deg;
    double lon_deg;
};
