#pragma once

/**
 * High-accuracy Geodetic <-> UTM projection.
 * Implements Snyder 6th-order series for WGS84. Precision ~1mm.
 */

struct UtmPoint {
    double easting;   // meters, false easting 500 000 applied
    double northing;  // meters, false northing 10 000 000 applied for S hemisphere
    int    zone;      // 1–60
    char   hemisphere; // 'N' or 'S'
};

/** Projects Lat/Lon to the corresponding UTM zone (auto-detected from longitude). */
UtmPoint ll_to_utm(double lat_deg, double lon_deg);

/** Back-projects a UTM coordinate to Geodetic Lat/Lon. */
void utm_to_ll(const UtmPoint& utm, double& lat_deg, double& lon_deg);
