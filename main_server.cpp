#include "query_handler.h"
#include "radar_server.h"
#include "dem_database.h"
#include <iostream>
#include <string>
#include <cstdlib>

static void usage(const char* prog)
{
    std::cerr << "Usage: " << prog
              << " [--lat <deg>] [--lon <deg>] [--alt <m>]"
                 " [--port <n>] [--tiles <dir>] [--max-range <m>]\n"
              << "Defaults: lat=32.08 lon=34.76 alt=0 port=8080 tiles=./tiles/ max-range=50000\n"
              << "  --alt   height of radar above local ground in meters (AGL)\n"
              << "          radar altitude MSL is resolved automatically from the DEM\n";
}

int main(int argc, char* argv[])
{
    LLA radar { 32.08, 34.76, 0.0 };
    double alt_agl = 0.0;
    int port = 8080;
    std::string tiles_dir = "./tiles/";
    LutConfig cfg;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") { usage(argv[0]); return 0; }
        if (i + 1 >= argc) { std::cerr << "Missing value for " << arg << "\n"; return 1; }
        std::string val = argv[++i];
        if      (arg == "--lat")       radar.lat_deg   = std::stod(val);
        else if (arg == "--lon")       radar.lon_deg   = std::stod(val);
        else if (arg == "--alt")       alt_agl         = std::stod(val);
        else if (arg == "--port")      port            = std::stoi(val);
        else if (arg == "--tiles")     tiles_dir       = val;
        else if (arg == "--max-range") cfg.max_range_m = std::stod(val);
        else { std::cerr << "Unknown argument: " << arg << "\n"; usage(argv[0]); return 1; }
    }

    // Resolve radar altitude MSL = terrain elevation at radar position + user-supplied AGL
    std::cout << "[server] Resolving radar terrain elevation...\n";
    std::cout.flush();
    {
        DemDatabase dem_tmp;
        dem_tmp.loadTilesAround(radar, cfg.max_range_m, tiles_dir, DemDatabase::Format::SRTM);
        double terrain = dem_tmp.getElevation(radar.lat_deg, radar.lon_deg);
        radar.alt_m = terrain + alt_agl;
        std::cout << "[server] Terrain at radar position: " << terrain << " m MSL\n";
        std::cout << "[server] Radar AGL: " << alt_agl << " m  →  Radar alt MSL: " << radar.alt_m << " m\n";
    }

    std::cout << "[server] Radar position: lat=" << radar.lat_deg
              << " lon=" << radar.lon_deg << " alt=" << radar.alt_m << " m MSL\n"
              << "[server] Max range: " << cfg.max_range_m << " m\n"
              << "[server] Tiles dir: " << tiles_dir << "\n"
              << "[server] Port: " << port << "\n";
    std::cout.flush();

    QueryHandler handler(radar, cfg, tiles_dir);
    RadarServer  server(handler, port);
    server.start();
    return 0;
}
