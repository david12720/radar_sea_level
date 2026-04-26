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

/**
 * Pure domain logic: RadarQuery → TargetResult.
 *
 * This class coordinates the radar measurement processing. On setRadar(), it builds
 * a high-density ground-elevation Look-Up Table (LUT) from the DEM and then releases
 * the heavy DEM tiles. This enables subsequent handle() calls to perform target
 * calculations with O(1) terrain lookup complexity and zero heap allocations.
 */
class QueryHandler {
public:
    QueryHandler(double max_range_m, const std::string& tiles_dir);

    /**
     * Sets the radar source position (MSL) and initializes the elevation map.
     * Triggers a LutExporter run to build a static grid around the radar.
     * @return false if no DEM tiles could be found for the given position.
     */
    bool setRadar(double lat_deg, double lon_deg, double alt_msl_m);

    /**
     * Computes target coordinates and altitudes for a specific measurement.
     * Performs range/azimuth validation and utilizes the pre-built LUT.
     * @throws RadarNotSetError if called before successful setRadar().
     * @throws ValidationError if measurement parameters are out of range.
     */
    TargetResult handle(const RadarQuery& q) const;

    /**
     * Point-query for terrain elevation.
     * Dynamically loads the specific 1° tile, samples it, and clears memory.
     * Used primarily for map-click inspection, not for high-speed tracking.
     */
    double getElevation(double lat_deg, double lon_deg);

    // Getters for current state and LUT data
    bool        radarSet()          const { return radar_set_; }
    const LLA&  radar()             const { return radar_; }
    double      maxRange()          const { return max_range_m_; }
    double      radarGroundElev()   const { return radar_ground_elev_m_; }

    /** Returns pointer to the static 14.4M-cell elevation grid. */
    const int32_t* lutCells()    const { return lut_cells_pool_; }

    /** Returns total number of cells currently populated in the LUT. */
    size_t         lutCellsSize() const { return static_cast<size_t>(lut_az_count_) * lut_range_count_; }

    /** Returns resolution and dimension metadata for the current LUT. */
    LutMetadata    lutMetadata() const;

private:
    double      max_range_m_;
    std::string tiles_dir_;
    LLA         radar_ {};
    bool        radar_set_            = false;
    double      radar_ground_elev_m_  = 0.0;

    static int32_t lut_cells_pool_[MAX_LUT_RANGES * MAX_LUT_AZIMUTHS];
    uint32_t       lut_az_count_    = 0;
    uint32_t       lut_range_count_ = 0;
    static constexpr double lut_az_step_deg_  = 0.1;
    static constexpr double lut_range_step_m_ = 15.0;

    DemDatabase dem_;

    void validate(const RadarQuery& q) const;
};
