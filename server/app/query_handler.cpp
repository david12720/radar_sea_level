#include "query_handler.h"
#include "lut_exporter.h"
#include "radar_compute.h"
#include "relative_angle.h"
#include "earth_model.h"
#include <cmath>
#include <cstring>
#include <algorithm>
#include <sstream>

int32_t QueryHandler::lut_cells_pool_[MAX_LUT_RANGES * MAX_LUT_AZIMUTHS];

QueryHandler::QueryHandler(double max_range_m, const std::string& tiles_dir)
    : max_range_m_(max_range_m), tiles_dir_(tiles_dir) {}

bool QueryHandler::setRadar(double lat_deg, double lon_deg, double alt_msl_m)
{
    radar_ = { lat_deg, lon_deg, alt_msl_m };

    LutExporter exporter(radar_, max_range_m_, tiles_dir_, LutExporter::AltMode::MSL);
    if (exporter.tilesLoaded() == 0) {
        radar_set_ = false;
        return false;
    }

    LutExportData data    = exporter.exportData();
    std::copy(data.cells, data.cells + data.total_cells, lut_cells_pool_);
    lut_az_count_         = data.az_count;
    lut_range_count_      = data.range_count;
    radar_ground_elev_m_  = exporter.radarTerrainM();
    radar_set_            = true;
    return true;
}

double QueryHandler::getElevation(double lat_deg, double lon_deg)
{
    int tile_lat = static_cast<int>(std::floor(lat_deg));
    int tile_lon = static_cast<int>(std::floor(lon_deg));
    std::string fpath = tiles_dir_ + DemDatabase::srtmFilename(tile_lat, tile_lon);
    double elev = 0.0;
    try {
        dem_.loadTile(fpath, tile_lat, tile_lon, DemDatabase::Format::SRTM);
        elev = dem_.getElevation(lat_deg, lon_deg);
    } catch (...) {}
    dem_.clear();
    return elev;
}

LutMetadata QueryHandler::lutMetadata() const
{
    return { lut_az_count_, lut_range_count_, lut_az_step_deg_, lut_range_step_m_ };
}

void QueryHandler::validate(const RadarQuery& q) const
{
    if (q.range_m < 0.0 || q.range_m > max_range_m_) {
        // Validation errors happen rarely; string allocation is acceptable for error reporting.
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
        const auto& model = getEarthModel(q.earth_model);
        TargetResult r = computeTargetSeaLevel(radar_, meas, q.ground_elevation_m, model);
        double elev_to_gnd = elevationToGround(radar_, r.horizontal_range_m, q.ground_elevation_m);
        r.relative_elevation_deg = relativeElevation(q.elevation_deg, elev_to_gnd);
        return r;
    } catch (const std::invalid_argument& e) {
        throw ValidationError(e.what());
    } catch (const std::runtime_error& e) {
        throw NoCoverageError(e.what());
    }
}
