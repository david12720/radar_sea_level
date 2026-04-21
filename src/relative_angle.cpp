#include "relative_angle.h"
#include "constants.h"
#include <cmath>

double elevationToGround(const LLA& radar,
                         double horizontal_range_m,
                         double ground_elevation_m)
{
    double vert = ground_elevation_m - radar.alt_m;
    return std::atan2(vert, horizontal_range_m) * RAD2DEG;
}

double relativeElevation(double beam_elevation_deg,
                         double elev_to_ground_deg)
{
    return beam_elevation_deg - elev_to_ground_deg;
}
