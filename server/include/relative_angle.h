#pragma once
#include "types.h"

// Elevation angle (degrees) from the radar to the ground point directly
// beneath a target — i.e. the line-of-sight to (target lat/lon, ground MSL).
double elevationToGround(const LLA& radar,
                         double horizontal_range_m,
                         double terrain_msl_m);

// Elevation angle of the radar beam above the terrain at the target's location.
// Positive  -> beam is above the ground (target is airborne or above terrain).
// Negative  -> beam is below the terrain line (ground masks the target).
double relativeElevation(double beam_elevation_deg,
                         double elev_to_ground_deg);
