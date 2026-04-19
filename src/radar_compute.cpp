#include "radar_compute.h"
#include "constants.h"
#include <cmath>

GroundPoint groundPoint(const LLA& radar, double horiz_range_m, double azimuth_deg)
{
    const double az_rad      = azimuth_deg * DEG2RAD;
    const double radar_lat_r = radar.lat_deg * DEG2RAD;
    return {
        radar.lat_deg + (horiz_range_m * std::cos(az_rad) / EARTH_RADIUS) * RAD2DEG,
        radar.lon_deg + (horiz_range_m * std::sin(az_rad) / (EARTH_RADIUS * std::cos(radar_lat_r))) * RAD2DEG
    };
}

static TargetResult computeGeometry(const LLA& radar, const RadarMeasurement& meas)
{
    const double el_rad      = meas.elevation_deg * DEG2RAD;
    const double horiz_range = meas.range_m * std::cos(el_rad);
    const double vert_offset = meas.range_m * std::sin(el_rad);
    GroundPoint gp           = groundPoint(radar, horiz_range, meas.azimuth_deg);

    TargetResult res;
    res.horizontal_range_m = horiz_range;
    res.vertical_offset_m  = vert_offset;
    res.position           = { gp.lat_deg, gp.lon_deg, radar.alt_m + vert_offset };
    return res;
}

TargetResult computeTargetSeaLevel(const LLA& radar,
                                   const RadarMeasurement& meas,
                                   const DemDatabase& dem)
{
    TargetResult res        = computeGeometry(radar, meas);
    res.ground_elevation_m  = dem.getElevation(res.position.lat_deg, res.position.lon_deg);
    res.target_height_agl_m = res.position.alt_m - res.ground_elevation_m;
    return res;
}

TargetResult computeTargetSeaLevel(const LLA& radar,
                                   const RadarMeasurement& meas,
                                   const ElevationLUT& lut)
{
    TargetResult res        = computeGeometry(radar, meas);
    res.ground_elevation_m  = lut.lookup(res.horizontal_range_m, meas.azimuth_deg);
    res.target_height_agl_m = res.position.alt_m - res.ground_elevation_m;
    return res;
}
