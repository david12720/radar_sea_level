#include "dem_database.h"
#include "elevation_lut.h"
#include "radar_compute.h"
#include "types.h"
#include <iostream>
#include <iomanip>
#include <fstream>

static void printResult(const LLA& radar, const RadarMeasurement& meas, const TargetResult& res)
{
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "======================================\n";
    std::cout << "  RADAR\n";
    std::cout << "    Lat / Lon  : " << radar.lat_deg << " / " << radar.lon_deg << " deg\n";
    std::cout << "    Altitude   : " << radar.alt_m << " m MSL\n";
    std::cout << "--------------------------------------\n";
    std::cout << "  MEASUREMENT\n";
    std::cout << "    Range      : " << meas.range_m       << " m\n";
    std::cout << "    Azimuth    : " << meas.azimuth_deg   << " deg\n";
    std::cout << "    Elevation  : " << meas.elevation_deg << " deg\n";
    std::cout << "--------------------------------------\n";
    std::cout << "  TARGET\n";
    std::cout << "    Lat / Lon        : " << res.position.lat_deg      << " / " << res.position.lon_deg << " deg\n";
    std::cout << "    Altitude MSL     : " << res.position.alt_m        << " m\n";
    std::cout << "    Ground Elev MSL  : " << res.ground_elevation_m    << " m  <- from DEM\n";
    std::cout << "    Height AGL       : " << res.target_height_agl_m   << " m\n";
    std::cout << "    Horiz Range      : " << res.horizontal_range_m    << " m\n";
    std::cout << "======================================\n";
}

int main()
{
    // ── Load DEM tiles ────────────────────────────────────────────────────────
    DemDatabase dem;
    const std::string tiles_dir = "./tiles/";

    for (int lat = 28; lat <= 33; ++lat) {
        for (int lon = 34; lon <= 35; ++lon) {
            std::string fpath = tiles_dir + DemDatabase::srtmFilename(lat, lon);
            if (std::ifstream(fpath).good())
                dem.loadTile(fpath, lat, lon, DemDatabase::Format::SRTM);
        }
    }

    // ── Radar position ────────────────────────────────────────────────────────
    LLA radar { 32.08, 34.76, 10.0 }; // Tel Aviv coast, 10 m altitude

    // ── Build LUT once at startup ─────────────────────────────────────────────
    LutConfig cfg;
    cfg.max_range_m  = 50000.0; // 50 km
    cfg.range_step_m = 15.0;
    cfg.az_step_deg  = 0.1;

    ElevationLUT lut;
    lut.build(radar, dem, cfg);

    // ── Tests ─────────────────────────────────────────────────────────────────
    std::cout << "\n--- Test 1: sea target 1000 m West, expect Ground Elev = 0 m ---\n";
    printResult(radar, { 1000.0, 270.0, 0.0 }, computeTargetSeaLevel(radar, { 1000.0, 270.0, 0.0 }, lut));

    std::cout << "\n--- Test 2: airborne 1000 m West, elev +2 deg, expect AGL ~ 35 m ---\n";
    printResult(radar, { 1000.0, 270.0, 2.0 }, computeTargetSeaLevel(radar, { 1000.0, 270.0, 2.0 }, lut));

    return 0;
}
