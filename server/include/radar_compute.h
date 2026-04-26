#pragma once
#include "types.h"
#include "dem_database.h"
#include "elevation_lut.h"
#include "earth_model.h"

/**
 * Core Physics Logic.
 * Solves for target coordinates (LLA) given radar measurements and an Earth model.
 */

/** Maps horizontal range + azimuth to a ground point using Flat-Earth projection. */
GroundPoint groundPoint(const LLA& radar, double horiz_range_m, double azimuth_deg);

/** 
 * Solves for target heights and positions. 
 * Overloads support three terrain data sources: DEM tiles, pre-calculated LUT, or direct MSL height.
 */
TargetResult computeTargetSeaLevel(const LLA& radar, const RadarMeasurement& meas,
                                   const DemDatabase& dem, const IEarthModel& model);

TargetResult computeTargetSeaLevel(const LLA& radar, const RadarMeasurement& meas,
                                   const ElevationLUT& lut, const IEarthModel& model);

TargetResult computeTargetSeaLevel(const LLA& radar, const RadarMeasurement& meas,
                                   double ground_elevation_m, const IEarthModel& model);

// Backward-compatible overloads (default to Flat Earth)
TargetResult computeTargetSeaLevel(const LLA& radar, const RadarMeasurement& meas, const DemDatabase& dem);
TargetResult computeTargetSeaLevel(const LLA& radar, const RadarMeasurement& meas, const ElevationLUT& lut);
TargetResult computeTargetSeaLevel(const LLA& radar, const RadarMeasurement& meas, double ground_elevation_m);
