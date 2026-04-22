#pragma once
#include "types.h"
#include "elevation_lut.h"
#include "dem_database.h"
#include <string>
#include <stdexcept>

struct RadarQuery {
    double range_m;
    double azimuth_deg;
    double elevation_deg;
};

// Thrown when the queried location falls outside loaded DEM tile coverage.
struct NoCoverageError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Thrown when input fields fail range validation.
struct ValidationError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Pure domain logic: RadarQuery → TargetResult.
// Owns the LUT; has no HTTP knowledge.
class QueryHandler {
public:
    // Loads tiles around radar and builds the LUT. Blocks until LUT is ready.
    QueryHandler(const LLA& radar, const LutConfig& cfg, const std::string& tiles_dir);

    // Validates query fields, performs LUT lookup, returns result.
    // Throws ValidationError on bad input, NoCoverageError if target is outside tile coverage.
    TargetResult handle(const RadarQuery& q) const;

    // Returns terrain elevation MSL at the given lat/lon, or 0 if outside tile coverage.
    double getElevation(double lat_deg, double lon_deg) const;

    const LLA&       radar()           const { return radar_; }
    const LutConfig& config()          const { return cfg_; }
    double           radarTerrainElev() const { return radar_terrain_elev_m_; }

private:
    LLA          radar_;
    LutConfig    cfg_;
    DemDatabase  dem_;
    ElevationLUT lut_;
    double       radar_terrain_elev_m_ = 0.0;

    void validate(const RadarQuery& q) const;
};
