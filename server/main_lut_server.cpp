#include "lut_tcp_server.h"
#include <iostream>
#include <string>

static void usage(const char* prog)
{
    std::cerr << "Usage: " << prog << " [--tiles <dir>] [--port <n>]\n"
              << "Defaults: tiles=./tiles/ port=9000\n"
              << "\nAccepts TCP connections. Each client sends a radar position + max range\n"
              << "and receives a raw int32[az][range] ground-elevation array (or saves to file).\n";
}

int main(int argc, char* argv[])
{
    std::string tiles_dir = "./tiles/";
    int         port      = 9000;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") { usage(argv[0]); return 0; }
        if (i + 1 >= argc) { std::cerr << "Missing value for " << arg << "\n"; return 1; }
        std::string val = argv[++i];
        if      (arg == "--tiles") tiles_dir = val;
        else if (arg == "--port")  port      = std::stoi(val);
        else { std::cerr << "Unknown argument: " << arg << "\n"; usage(argv[0]); return 1; }
    }

    std::cout << "[lut_tcp] Tiles dir: " << tiles_dir << "\n"
              << "[lut_tcp] Port:      " << port      << "\n";
    std::cout.flush();

    LutTcpServer server(tiles_dir, port);
    server.start();
    return 0;
}
