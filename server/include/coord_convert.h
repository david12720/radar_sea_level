#pragma once

struct UtmPoint {
    double easting;   // meters, false easting 500 000 applied
    double northing;  // meters, false northing 10 000 000 applied for S hemisphere
    int    zone;      // 1–60
    char   hemisphere; // 'N' or 'S'
};

// WGS84 lat/lon (degrees) → UTM.  Zone is auto-detected from longitude.
UtmPoint ll_to_utm(double lat_deg, double lon_deg);

// UTM → WGS84 lat/lon (degrees).
// hemisphere: 'N' or 'S'  (case-insensitive)
void utm_to_ll(const UtmPoint& utm, double& lat_deg, double& lon_deg);
