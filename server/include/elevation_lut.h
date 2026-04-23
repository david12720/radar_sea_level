#pragma once
#include "constants.h"
#include "types.h"
#include "dem_database.h"

struct LutConfig {
    double max_range_m  = 50000.0; // meters
    // NOTE: SRTM-3 post spacing is ~90 m. A range_step_m finer than that
    // adds LUT bins without adding real terrain detail — consider ~90 m
    // if memory or build time is a concern (6× reduction).
    // az_step_deg = 0.1° ≈ 105 m arc at 60 km — well matched to SRTM-3.
    double range_step_m = 15.0;    // meters per range bin
    double az_step_deg  = 0.1;     // degrees per azimuth bin
};

// Pre-computed ground elevation table indexed by [range_bin][azimuth_bin].
// Built once at startup from a DemDatabase; O(1) lookup at runtime.
class ElevationLUT {
public:
    // Populates the table for the given radar position and coverage config.
    void build(const LLA& radar, const DemDatabase& dem, const LutConfig& cfg);

    // Returns ground elevation for a slant-range measurement.
    // horiz_range_m = slant_range * cos(elevation); azimuth_deg in [0, 360).
    double lookup(double horiz_range_m, double azimuth_deg) const;

    int numRanges()        const { return num_ranges_;   }
    int numAzimuths()      const { return num_azimuths_; }
    const LutConfig& config() const { return cfg_;       }

private:
    LutConfig                       cfg_;
    LLA                             radar_;
    int                             num_ranges_   = 0;
    int                             num_azimuths_ = 0;
    static float                    table_[MAX_LUT_RANGES][MAX_LUT_AZIMUTHS];
};
