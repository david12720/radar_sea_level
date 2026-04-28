#include "radar_compute.h"
#include "constants.h"
#include <cmath>
#include <memory>

// Flat-Earth projection of a horizontal range + azimuth onto the ground.
// Latitude offset is uniform (1 rad = EARTH_RADIUS m).
// Longitude offset shrinks toward the poles: 1° lon = cos(lat) * 1° lat in metres.
GroundPoint groundPoint(const LLA& radar, double horiz_range_m, double azimuth_deg)
{
    const double az_rad      = azimuth_deg * DEG2RAD;
    const double radar_lat_r = radar.lat_deg * DEG2RAD;
    return {
        radar.lat_deg + (horiz_range_m * std::cos(az_rad) / EARTH_RADIUS) * RAD2DEG,
        radar.lon_deg + (horiz_range_m * std::sin(az_rad) / (EARTH_RADIUS * std::cos(radar_lat_r))) * RAD2DEG
    };
}

static const IEarthModel& flat_model()
{
    return getEarthModel("flat");
}

// ── Model-aware overloads ────────────────────────────────────────────────────

TargetResult computeTargetSeaLevel(const LLA& radar,
                                   const RadarMeasurement& meas,
                                   const DemDatabase& dem,
                                   const IEarthModel& model)
{
    TargetResult res        = model.propagate(radar, meas);
    res.terrain_msl_m  = dem.getElevation(res.position.lat_deg, res.position.lon_deg);
    res.target_height_agl_m = res.position.alt_m - res.terrain_msl_m;
    return res;
}

TargetResult computeTargetSeaLevel(const LLA& radar,
                                   const RadarMeasurement& meas,
                                   const ElevationLUT& lut,
                                   const IEarthModel& model)
{
    TargetResult res        = model.propagate(radar, meas);
    res.terrain_msl_m  = lut.lookup(res.horizontal_range_m, meas.azimuth_deg);
    res.target_height_agl_m = res.position.alt_m - res.terrain_msl_m;
    return res;
}

TargetResult computeTargetSeaLevel(const LLA& radar,
                                   const RadarMeasurement& meas,
                                   double terrain_msl_m,
                                   const IEarthModel& model)
{
    TargetResult res        = model.propagate(radar, meas);
    res.terrain_msl_m  = terrain_msl_m;
    res.target_height_agl_m = res.position.alt_m - terrain_msl_m;
    return res;
}

// ── Flat-Earth overloads (delegate to model-aware versions) ──────────────────

TargetResult computeTargetSeaLevel(const LLA& radar, const RadarMeasurement& meas,
                                   const DemDatabase& dem)
{
    return computeTargetSeaLevel(radar, meas, dem, flat_model());
}

TargetResult computeTargetSeaLevel(const LLA& radar, const RadarMeasurement& meas,
                                   const ElevationLUT& lut)
{
    return computeTargetSeaLevel(radar, meas, lut, flat_model());
}

TargetResult computeTargetSeaLevel(const LLA& radar, const RadarMeasurement& meas,
                                   double terrain_msl_m)
{
    return computeTargetSeaLevel(radar, meas, terrain_msl_m, flat_model());
}
