#include "target_tcp_server.h"
#include <iostream>
#include <string>

static void usage(const char* prog)
{
    std::cerr << "Usage: " << prog << " [--port <n>]\n"
              << "Default: port=9001\n"
              << "\nAccepts TCP connections for high-rate target queries.\n"
              << "Session setup: send radar position (lat, lon, alt_msl) as 3 doubles.\n"
              << "Per target: send (range_m, azimuth_deg, elevation_deg, ground_elevation_m) as 4 doubles.\n"
              << "Response:   receive (lat_deg, lon_deg, alt_msl_m, agl_m, relative_elev_deg) as 5 doubles.\n"
              << "Disconnect to end session.\n";
}

int main(int argc, char* argv[])
{
    int port = 9001;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") { usage(argv[0]); return 0; }
        if (i + 1 >= argc) { std::cerr << "Missing value for " << arg << "\n"; return 1; }
        std::string val = argv[++i];
        if (arg == "--port") port = std::stoi(val);
        else { std::cerr << "Unknown argument: " << arg << "\n"; usage(argv[0]); return 1; }
    }

    std::cout << "[target_tcp] Port: " << port << "\n";
    std::cout.flush();

    TargetTcpServer server(port);
    server.start();
    return 0;
}
