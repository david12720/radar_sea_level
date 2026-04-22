#include "query_handler.h"
#include "radar_compute.h"
#include "relative_angle.h"
#include <sstream>

QueryHandler::QueryHandler(const LLA& radar, const LutConfig& cfg, const std::string& tiles_dir)
    : radar_(radar), cfg_(cfg)
{
    dem_.loadTilesAround(radar_, cfg_.max_range_m, tiles_dir, DemDatabase::Format::SRTM);
    // radar.alt_m is treated as AGL; resolve MSL from terrain
    try {
        radar_terrain_elev_m_ = dem_.getElevation(radar_.lat_deg, radar_.lon_deg);
    } catch (...) {
        radar_terrain_elev_m_ = 0.0;
    }
    radar_.alt_m = radar_terrain_elev_m_ + radar.alt_m;
    lut_.build(radar_, dem_, cfg_);
}

double QueryHandler::getElevation(double lat_deg, double lon_deg) const
{
    try {
        return dem_.getElevation(lat_deg, lon_deg);
    } catch (...) {
        return 0.0; // outside tile coverage — sea level
    }
}

void QueryHandler::validate(const RadarQuery& q) const
{
    if (q.range_m < 0.0 || q.range_m > cfg_.max_range_m) {
        std::ostringstream oss;
        oss << "range_m must be in [0, " << cfg_.max_range_m << "]";
        throw ValidationError(oss.str());
    }
    if (q.azimuth_deg < 0.0 || q.azimuth_deg >= 360.0)
        throw ValidationError("azimuth_deg must be in [0, 360)");
    if (q.elevation_deg < -10.0 || q.elevation_deg > 45.0)
        throw ValidationError("elevation_deg must be in [-10, 45]");
}

TargetResult QueryHandler::handle(const RadarQuery& q) const
{
    validate(q);
    RadarMeasurement meas { q.range_m, q.azimuth_deg, q.elevation_deg };
    try {
        TargetResult r = computeTargetSeaLevel(radar_, meas, lut_);
        double elev_to_gnd = elevationToGround(radar_, r.horizontal_range_m, r.ground_elevation_m);
        r.relative_elevation_deg = relativeElevation(q.elevation_deg, elev_to_gnd);
        return r;
    } catch (const std::runtime_error& e) {
        throw NoCoverageError(e.what());
    }
}
