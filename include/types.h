#pragma once

struct LLA {
    double lat_deg;
    double lon_deg;
    double alt_m;
};

struct RadarMeasurement {
    double range_m;
    double azimuth_deg;   // True North clockwise
    double elevation_deg; // Positive up from horizon
};

struct TargetResult {
    LLA    position;             // Target Lat/Lon/Alt MSL
    double ground_elevation_m;   // Terrain height MSL at target Lat/Lon
    double target_height_agl_m;  // Target altitude above ground level
    double horizontal_range_m;
    double vertical_offset_m;
    double relative_elevation_deg; // Beam elevation above terrain line at target (deg)
};

struct GroundPoint {
    double lat_deg;
    double lon_deg;
};
