#include "elevation_lut.h"
#include "radar_compute.h"
#include <iostream>
#include <cmath>
#include <algorithm>

void ElevationLUT::build(const LLA& radar, const DemDatabase& dem, const LutConfig& cfg)
{
    cfg_          = cfg;
    radar_        = radar;
    num_ranges_   = static_cast<int>(std::ceil(cfg.max_range_m / cfg.range_step_m)) + 1;
    num_azimuths_ = static_cast<int>(std::round(360.0 / cfg.az_step_deg));

    table_.assign(num_ranges_, std::vector<float>(num_azimuths_, 0.0f));

    int skipped = 0;
    for (int ri = 0; ri < num_ranges_; ++ri) {
        const double horiz_range = ri * cfg.range_step_m;
        for (int ai = 0; ai < num_azimuths_; ++ai) {
            const double azimuth = ai * cfg.az_step_deg;
            GroundPoint gp = groundPoint(radar, horiz_range, azimuth);
            if (dem.hasTile(gp.lat_deg, gp.lon_deg))
                table_[ri][ai] = static_cast<float>(dem.getElevation(gp.lat_deg, gp.lon_deg));
            else
                ++skipped; // default 0.0f = sea level
        }
    }

    std::cout << "[LUT] Built " << num_ranges_ << " × " << num_azimuths_
              << " table (" << (num_ranges_ * num_azimuths_ * 4 / 1024 / 1024) << " MB)"
              << ", " << skipped << " cells defaulted to 0 (no tile)\n";
}

double ElevationLUT::lookup(double horiz_range_m, double azimuth_deg) const
{
    int ri = static_cast<int>(horiz_range_m / cfg_.range_step_m + 0.5);
    ri = std::clamp(ri, 0, num_ranges_ - 1);

    azimuth_deg = std::fmod(azimuth_deg, 360.0);
    if (azimuth_deg < 0.0) azimuth_deg += 360.0;
    int ai = static_cast<int>(azimuth_deg / cfg_.az_step_deg + 0.5) % num_azimuths_;

    return static_cast<double>(table_[ri][ai]);
}
