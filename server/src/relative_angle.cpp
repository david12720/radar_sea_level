#include "relative_angle.h"
#include "constants.h"
#include <cmath>

// Elevation angle (deg) from the radar to the ground point beneath the target.
// vert = ground_elev - radar_alt: negative when the terrain is below the radar,
// so the returned angle will be negative (looking down to reach the ground).
double elevationToGround(const LLA& radar,
                         double horizontal_range_m,
                         double ground_elevation_m)
{
    double vert = ground_elevation_m - radar.alt_m;
    return std::atan2(vert, horizontal_range_m) * RAD2DEG;
}

// Relative elevation = how many degrees the beam sits above the terrain line.
// beam_elevation_deg - elev_to_ground_deg:
//   > 0 : beam above terrain (target is airborne or terrain is depressed)
//   = 0 : beam grazes the terrain surface
//   < 0 : terrain masks the target (ground is higher than the beam)
double relativeElevation(double beam_elevation_deg,
                         double elev_to_ground_deg)
{
    return beam_elevation_deg - elev_to_ground_deg;
}
