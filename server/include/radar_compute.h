#pragma once
#include "types.h"
#include "dem_database.h"
#include "elevation_lut.h"

// Flat-Earth ground projection of a horizontal range + azimuth from radar position.
GroundPoint groundPoint(const LLA& radar, double horiz_range_m, double azimuth_deg);

// Compute target position and AGL height — queries DEM directly (accurate, slower).
TargetResult computeTargetSeaLevel(const LLA& radar,
                                   const RadarMeasurement& meas,
                                   const DemDatabase& dem);

// Compute target position and AGL height — uses pre-built LUT (O(1), for real-time use).
TargetResult computeTargetSeaLevel(const LLA& radar,
                                   const RadarMeasurement& meas,
                                   const ElevationLUT& lut);

// Compute target position and AGL height — ground elevation provided by caller.
// Used when the client already holds a height map (no DEM or LUT access needed).
TargetResult computeTargetSeaLevel(const LLA& radar,
                                   const RadarMeasurement& meas,
                                   double ground_elevation_m);
