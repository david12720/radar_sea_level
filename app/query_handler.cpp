#include "query_handler.h"
#include "radar_compute.h"
#include <sstream>

QueryHandler::QueryHandler(const LLA& radar, const LutConfig& cfg, const std::string& tiles_dir)
    : radar_(radar), cfg_(cfg)
{
    DemDatabase dem;
    dem.loadTilesAround(radar_, cfg_.max_range_m, tiles_dir, DemDatabase::Format::SRTM);
    lut_.build(radar_, dem, cfg_);
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
        return computeTargetSeaLevel(radar_, meas, lut_);
    } catch (const std::runtime_error& e) {
        throw NoCoverageError(e.what());
    }
}
