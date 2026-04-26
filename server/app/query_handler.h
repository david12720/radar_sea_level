#pragma once
#include "types.h"
#include "dem_database.h"
#include <cstdint>
#include <string>
#include <stdexcept>
#include <vector>

struct RadarQuery {
    double      range_m;
    double      azimuth_deg;
    double      elevation_deg;
    std::string earth_model;
    double      ground_elevation_m = 0.0;
};

struct NoCoverageError : std::runtime_error {
    using std::runtime_error::runtime_error;
};
struct ValidationError : std::runtime_error {
    using std::runtime_error::runtime_error;
};
struct RadarNotSetError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct LutMetadata {
    uint32_t az_count;
    uint32_t range_count;
    double   az_step_deg;
    double   range_step_m;
};

// Pure domain logic: RadarQuery → TargetResult.
// On setRadar(), builds a ground-elevation LUT from DEM and releases tiles.
// Subsequent handle() calls use caller-supplied ground_elevation_m (from the LUT).
class QueryHandler {
public:
    QueryHandler(double max_range_m, const std::string& tiles_dir);

    // Loads DEM, builds ground-elevation LUT, releases DEM, caches LUT.
    // Returns false if no tiles could be loaded.
    bool setRadar(double lat_deg, double lon_deg, double alt_msl_m);

    // Validates query, computes target geometry using q.ground_elevation_m.
    // Throws RadarNotSetError, ValidationError.
    TargetResult handle(const RadarQuery& q) const;

    // Per-request DEM load: loads the single tile covering (lat,lon), returns elevation, releases.
    double getElevation(double lat_deg, double lon_deg);

    bool        radarSet()          const { return radar_set_; }
    const LLA&  radar()             const { return radar_; }
    double      maxRange()          const { return max_range_m_; }
    double      radarGroundElev()   const { return radar_ground_elev_m_; }

    const std::vector<int32_t>& lutCells()    const { return lut_cells_; }
    LutMetadata                 lutMetadata() const;

private:
    double      max_range_m_;
    std::string tiles_dir_;
    LLA         radar_ {};
    bool        radar_set_            = false;
    double      radar_ground_elev_m_  = 0.0;

    std::vector<int32_t> lut_cells_;
    uint32_t             lut_az_count_    = 0;
    uint32_t             lut_range_count_ = 0;
    static constexpr double lut_az_step_deg_  = 0.1;
    static constexpr double lut_range_step_m_ = 15.0;

    DemDatabase dem_;

    void validate(const RadarQuery& q) const;
};
