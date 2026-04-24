#include "query_handler.h"
#include "radar_compute.h"
#include "relative_angle.h"
#include <sstream>

QueryHandler::QueryHandler(double max_range_m, const std::string& tiles_dir)
    : max_range_m_(max_range_m), tiles_dir_(tiles_dir) {}

bool QueryHandler::setRadar(double lat_deg, double lon_deg, double alt_msl_m)
{
    radar_ = { lat_deg, lon_deg, alt_msl_m };
    int loaded = dem_.loadTilesAround(radar_, max_range_m_, tiles_dir_, DemDatabase::Format::SRTM);
    radar_set_ = (loaded > 0);
    return radar_set_;
}

double QueryHandler::getElevation(double lat_deg, double lon_deg) const
{
    try {
        return dem_.getElevation(lat_deg, lon_deg);
    } catch (...) {
        return 0.0;
    }
}

void QueryHandler::validate(const RadarQuery& q) const
{
    if (q.range_m < 0.0 || q.range_m > max_range_m_) {
        std::ostringstream oss;
        oss << "range_m must be in [0, " << max_range_m_ << "]";
        throw ValidationError(oss.str());
    }
    if (q.azimuth_deg < 0.0 || q.azimuth_deg >= 360.0)
        throw ValidationError("azimuth_deg must be in [0, 360)");
    if (q.elevation_deg < -10.0 || q.elevation_deg > 45.0)
        throw ValidationError("elevation_deg must be in [-10, 45]");
}

TargetResult QueryHandler::handle(const RadarQuery& q) const
{
    if (!radar_set_)
        throw RadarNotSetError("radar position not set — call POST /radar first");
    validate(q);
    RadarMeasurement meas { q.range_m, q.azimuth_deg, q.elevation_deg };
    try {
        TargetResult r = computeTargetSeaLevel(radar_, meas, dem_);
        double elev_to_gnd = elevationToGround(radar_, r.horizontal_range_m, r.ground_elevation_m);
        r.relative_elevation_deg = relativeElevation(q.elevation_deg, elev_to_gnd);
        return r;
    } catch (const std::runtime_error& e) {
        throw NoCoverageError(e.what());
    }
}
