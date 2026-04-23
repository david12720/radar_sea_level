#include "lut_exporter.h"
#include "radar_compute.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

LutExporter::LutExporter(const LLA& radar, double max_range_m, const std::string& tiles_dir)
    : radar_(radar), max_range_m_(max_range_m)
{
    int n = dem_.loadTilesAround(radar_, max_range_m, tiles_dir, DemDatabase::Format::SRTM);
    std::cout << "[lut_export] Tiles loaded: " << n << " from " << tiles_dir << "\n";

    // radar_.alt_m is AGL; resolve MSL from terrain
    double terrain = 0.0;
    try { terrain = dem_.getElevation(radar_.lat_deg, radar_.lon_deg); } catch (...) {}
    radar_.alt_m = terrain + radar.alt_m;
    std::cout << "[lut_export] Radar terrain: " << terrain << " m MSL  |  alt MSL: " << radar_.alt_m << " m\n";
    std::cout.flush();
}

LutExportData LutExporter::exportData() const
{
    const uint32_t az_count    = static_cast<uint32_t>(std::round(360.0 / az_step_deg_));
    const uint32_t range_count = static_cast<uint32_t>(std::ceil(max_range_m_ / range_step_m_)) + 1;

    LutExportData data;
    data.az_count    = az_count;
    data.range_count = range_count;
    data.cells.resize(az_count * range_count);

    for (uint32_t ai = 0; ai < az_count; ++ai) {
        const double azimuth = ai * az_step_deg_;
        for (uint32_t ri = 0; ri < range_count; ++ri) {
            const double range = ri * range_step_m_;
            GroundPoint gp = groundPoint(radar_, range, azimuth);
            double elev = dem_.hasTile(gp.lat_deg, gp.lon_deg)
                        ? dem_.getElevation(gp.lat_deg, gp.lon_deg)
                        : 0.0;
            data.cells[ai * range_count + ri] = static_cast<int32_t>(std::round(elev));
        }
    }
    return data;
}

void LutExporter::saveToFile(const LutExportData& data, const std::string& path) const
{
    std::ofstream f(path, std::ios::binary);
    if (!f)
        throw std::runtime_error("cannot open output file: " + path);
    f.write(reinterpret_cast<const char*>(data.cells.data()),
            static_cast<std::streamsize>(data.cells.size() * sizeof(int32_t)));
}
