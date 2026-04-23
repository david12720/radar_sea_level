#include "lut_exporter.h"
#include "radar_compute.h"
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

int32_t LutExporter::results_buffer_[MAX_LUT_RANGES * MAX_LUT_AZIMUTHS];

LutExporter::LutExporter(const LLA& radar, double max_range_m, const std::string& tiles_dir)
    : radar_(radar), max_range_m_(max_range_m)
{
    auto t0 = std::chrono::steady_clock::now();
    int n = dem_.loadTilesAround(radar_, max_range_m, tiles_dir, DemDatabase::Format::SRTM);
    auto t1 = std::chrono::steady_clock::now();
    std::cout << "[lut_export] Tiles loaded: " << n << " from " << tiles_dir
              << "  (" << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count() << " ms)\n";

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

    if (static_cast<size_t>(az_count) * range_count > MAX_LUT_RANGES * MAX_LUT_AZIMUTHS) {
        throw std::runtime_error("Export grid exceeds static buffer size");
    }

    LutExportData data;
    data.az_count    = az_count;
    data.range_count = range_count;
    data.total_cells = static_cast<size_t>(az_count) * range_count;
    data.cells       = results_buffer_;

    auto t0 = std::chrono::steady_clock::now();

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

    auto t1 = std::chrono::steady_clock::now();

    // timing code simplified or removed if preferred, keeping for now
    std::cout << "[lut_export] Grid build: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count() << " ms"
              << "  cells=" << az_count << "x" << range_count << "\n";
    std::cout.flush();

    return data;
}

void LutExporter::saveToFile(const LutExportData& data, const std::string& path) const
{
    std::ofstream f(path, std::ios::binary);
    if (!f)
        throw std::runtime_error("cannot open output file: " + path);
    f.write(reinterpret_cast<const char*>(data.cells),
            static_cast<std::streamsize>(data.total_cells * sizeof(int32_t)));
}
