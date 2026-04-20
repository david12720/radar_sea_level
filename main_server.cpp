#include "query_handler.h"
#include "radar_server.h"
#include <iostream>
#include <string>
#include <cstdlib>

static void usage(const char* prog)
{
    std::cerr << "Usage: " << prog
              << " [--lat <deg>] [--lon <deg>] [--alt <m>]"
                 " [--port <n>] [--tiles <dir>] [--max-range <m>]\n"
              << "Defaults: lat=32.08 lon=34.76 alt=10 port=8080 tiles=./tiles/ max-range=50000\n";
}

int main(int argc, char* argv[])
{
    LLA radar { 32.08, 34.76, 10.0 };
    int port = 8080;
    std::string tiles_dir = "./tiles/";
    LutConfig cfg;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") { usage(argv[0]); return 0; }
        if (i + 1 >= argc) { std::cerr << "Missing value for " << arg << "\n"; return 1; }
        std::string val = argv[++i];
        if      (arg == "--lat")       radar.lat_deg     = std::stod(val);
        else if (arg == "--lon")       radar.lon_deg     = std::stod(val);
        else if (arg == "--alt")       radar.alt_m       = std::stod(val);
        else if (arg == "--port")      port              = std::stoi(val);
        else if (arg == "--tiles")     tiles_dir         = val;
        else if (arg == "--max-range") cfg.max_range_m   = std::stod(val);
        else { std::cerr << "Unknown argument: " << arg << "\n"; usage(argv[0]); return 1; }
    }

    std::cout << "[server] Radar position: lat=" << radar.lat_deg
              << " lon=" << radar.lon_deg << " alt=" << radar.alt_m << " m\n"
              << "[server] Max range: " << cfg.max_range_m << " m\n"
              << "[server] Tiles dir: " << tiles_dir << "\n";

    QueryHandler handler(radar, cfg, tiles_dir);
    RadarServer  server(handler, port);
    server.start();
    return 0;
}
