#include "tdp_dtm_tcp_server.h"
#include "dem_database.h"
#include <iostream>
#include <string>

static void usage(const char* prog)
{
    std::cerr << "Usage: " << prog
              << " [--tiles <dir>] [--port <n>] [--max-range <m>] [--dem-format hgt|dted]\n"
              << "Defaults: tiles=./tiles/ port=9001 max-range=15000 dem-format=hgt\n"
              << "\nAccepts TCP connections. Each client sends a UTM radar position\n"
              << "and receives a terrain MSL response + saves a full LUT to output.bin.\n";
}

int main(int argc, char* argv[])
{
    std::string         tiles_dir = "./tiles/";
    int                 port      = 9001;
    double              max_range = 15000.0;
    DemDatabase::Format dem_fmt   = DemDatabase::Format::SRTM;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") { usage(argv[0]); return 0; }
        if (i + 1 >= argc) { std::cerr << "Missing value for " << arg << "\n"; return 1; }
        std::string val = argv[++i];
        if      (arg == "--tiles")      tiles_dir = val;
        else if (arg == "--port")       port      = std::stoi(val);
        else if (arg == "--max-range")  max_range = std::stod(val);
        else if (arg == "--dem-format") {
            if      (val == "dted") dem_fmt = DemDatabase::Format::DTED2;
            else if (val == "hgt")  dem_fmt = DemDatabase::Format::SRTM;
            else { std::cerr << "Unknown dem-format: " << val << " (use hgt or dted)\n"; return 1; }
        }
        else { std::cerr << "Unknown argument: " << arg << "\n"; usage(argv[0]); return 1; }
    }

    std::cout << "[tdp_dtm] Tiles dir:  " << tiles_dir << "\n"
              << "[tdp_dtm] Port:       " << port      << "\n"
              << "[tdp_dtm] Max range:  " << max_range << " m\n"
              << "[tdp_dtm] DEM format: " << (dem_fmt == DemDatabase::Format::DTED2 ? "dted" : "hgt") << "\n";
    std::cout.flush();

    TdpDtmTcpServer server(tiles_dir, port, max_range, dem_fmt);
    server.start();
    return 0;
}
