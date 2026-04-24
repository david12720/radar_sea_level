#include "query_handler.h"
#include "radar_server.h"
#include <iostream>
#include <string>

static void usage(const char* prog)
{
    std::cerr << "Usage: " << prog
              << " [--port <n>] [--tiles <dir>] [--max-range <m>]\n"
              << "Defaults: port=8080 tiles=./tiles/ max-range=50000\n"
              << "\nSet radar position at runtime via:  POST /radar\n"
              << "  Body: {\"lat_deg\":<deg>, \"lon_deg\":<deg>, \"alt_msl_m\":<m>}\n";
}

int main(int argc, char* argv[])
{
    int port = 8080;
    std::string tiles_dir = "./tiles/";
    double max_range_m = 50000.0;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") { usage(argv[0]); return 0; }
        if (i + 1 >= argc) { std::cerr << "Missing value for " << arg << "\n"; return 1; }
        std::string val = argv[++i];
        if      (arg == "--port")      port         = std::stoi(val);
        else if (arg == "--tiles")     tiles_dir    = val;
        else if (arg == "--max-range") max_range_m  = std::stod(val);
        else { std::cerr << "Unknown argument: " << arg << "\n"; usage(argv[0]); return 1; }
    }

    std::cout << "[server] Max range: " << max_range_m << " m\n"
              << "[server] Tiles dir: " << tiles_dir << "\n"
              << "[server] Port:      " << port << "\n"
              << "[server] Waiting for radar position via POST /radar\n";
    std::cout.flush();

    QueryHandler handler(max_range_m, tiles_dir);
    RadarServer server(handler, port);
    server.start();
    return 0;
}
