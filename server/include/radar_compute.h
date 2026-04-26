#pragma once
#include "types.h"
#include "dem_database.h"
#include "elevation_lut.h"
#include "earth_model.h"

// Flat-Earth ground projection of a horizontal range + azimuth from radar position.
GroundPoint groundPoint(const LLA& radar, double horiz_range_m, double azimuth_deg);

// ── Flat-Earth overloads (backward-compatible) ───────────────────────────────

TargetResult computeTargetSeaLevel(const LLA& radar,
                                   const RadarMeasurement& meas,
                                   const DemDatabase& dem);

TargetResult computeTargetSeaLevel(const LLA& radar,
                                   const RadarMeasurement& meas,
                                   const ElevationLUT& lut);

TargetResult computeTargetSeaLevel(const LLA& radar,
                                   const RadarMeasurement& meas,
                                   double ground_elevation_m);

// ── Model-aware overloads ────────────────────────────────────────────────────
// The supplied IEarthModel decides how (range, azimuth, elevation) map to
// (target lat/lon/alt, horizontal_range_m, vertical_offset_m).

TargetResult computeTargetSeaLevel(const LLA& radar,
                                   const RadarMeasurement& meas,
                                   const DemDatabase& dem,
                                   const IEarthModel& model);

TargetResult computeTargetSeaLevel(const LLA& radar,
                                   const RadarMeasurement& meas,
                                   const ElevationLUT& lut,
                                   const IEarthModel& model);

TargetResult computeTargetSeaLevel(const LLA& radar,
                                   const RadarMeasurement& meas,
                                   double ground_elevation_m,
                                   const IEarthModel& model);
