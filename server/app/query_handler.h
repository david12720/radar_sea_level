#pragma once
#include "types.h"
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

// Thrown when a query is issued before a radar position has been set.
struct RadarNotSetError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Pure domain logic: RadarQuery → TargetResult.
// Owns the DEM; has no HTTP knowledge.
// Radar position is set dynamically via setRadar() rather than at construction.
class QueryHandler {
public:
    QueryHandler(double max_range_m, const std::string& tiles_dir);

    // Sets radar position (MSL) and loads DEM tiles covering that position.
    // Returns false if no tiles could be loaded (out of coverage area).
    bool setRadar(double lat_deg, double lon_deg, double alt_msl_m);

    // Validates query, looks up ground elevation from DEM, returns result.
    // Throws RadarNotSetError if setRadar() has not been called.
    // Throws ValidationError on bad input, NoCoverageError outside tile coverage.
    TargetResult handle(const RadarQuery& q) const;

    // Returns terrain elevation MSL at the given lat/lon, or 0 if outside coverage.
    double getElevation(double lat_deg, double lon_deg) const;

    bool        radarSet()          const { return radar_set_; }
    const LLA&  radar()             const { return radar_; }
    double      maxRange()          const { return max_range_m_; }

private:
    double      max_range_m_;
    std::string tiles_dir_;
    LLA         radar_ {};
    bool        radar_set_ = false;
    DemDatabase dem_;

    void validate(const RadarQuery& q) const;
};
