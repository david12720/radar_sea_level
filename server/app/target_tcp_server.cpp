#include "target_tcp_server.h"
#include "radar_compute.h"
#include "relative_angle.h"
#include "types.h"
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>

#ifdef _WIN32
#  include <winsock2.h>
#  pragma comment(lib, "ws2_32.lib")
   using socket_t = SOCKET;
#  define CLOSE_SOCKET(s) closesocket(s)
#  define INVALID_SOCK    INVALID_SOCKET
#else
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <unistd.h>
   using socket_t = int;
#  define CLOSE_SOCKET(s) close(s)
#  define INVALID_SOCK    (-1)
#endif

#pragma pack(push, 1)
struct SessionHeader {
    double radar_lat_deg;
    double radar_lon_deg;
    double radar_alt_msl_m;
};

struct TargetRequest {
    double range_m;
    double azimuth_deg;
    double elevation_deg;
    double terrain_msl_m;
};

struct TargetResponse {
    double lat_deg;
    double lon_deg;
    double alt_msl_m;
    double agl_m;
    double relative_elev_deg;
};
#pragma pack(pop)

static bool recvAll(socket_t s, void* buf, size_t len)
{
    char* p = reinterpret_cast<char*>(buf);
    while (len > 0) {
        int n = recv(s, p, static_cast<int>(len), 0);
        if (n <= 0) return false;
        p   += n;
        len -= static_cast<size_t>(n);
    }
    return true;
}

static bool sendAll(socket_t s, const void* buf, size_t len)
{
    const char* p = reinterpret_cast<const char*>(buf);
    while (len > 0) {
        int n = send(s, p, static_cast<int>(len), 0);
        if (n <= 0) return false;
        p   += n;
        len -= static_cast<size_t>(n);
    }
    return true;
}

TargetTcpServer::TargetTcpServer(int port) : port_(port) {}

#ifdef _WIN32
void TargetTcpServer::handleClient(unsigned long long sock) const
#else
void TargetTcpServer::handleClient(int sock) const
#endif
{
    SessionHeader session{};
    if (!recvAll(sock, &session, sizeof(session))) return;

    LLA radar { session.radar_lat_deg, session.radar_lon_deg, session.radar_alt_msl_m };

    TargetRequest req{};
    while (recvAll(sock, &req, sizeof(req))) {
        RadarMeasurement meas { req.range_m, req.azimuth_deg, req.elevation_deg };
        TargetResult r = computeTargetSeaLevel(radar, meas, req.terrain_msl_m);

        double elev_to_gnd = elevationToGround(radar, r.horizontal_range_m, req.terrain_msl_m);
        r.relative_elevation_deg = relativeElevation(req.elevation_deg, elev_to_gnd);

        TargetResponse resp {
            r.position.lat_deg,
            r.position.lon_deg,
            r.position.alt_m,
            r.target_height_agl_m,
            r.relative_elevation_deg
        };
        if (!sendAll(sock, &resp, sizeof(resp))) return;
    }
}

void TargetTcpServer::start()
{
#ifdef _WIN32
    WSADATA wsa{};
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    socket_t server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == INVALID_SOCK)
        throw std::runtime_error("socket() failed");

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(static_cast<uint16_t>(port_));

    if (bind(server_sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
        throw std::runtime_error("bind() failed on port " + std::to_string(port_));

    listen(server_sock, 4);
    std::cout << "[target_tcp] Listening on port " << port_ << " -- Ctrl-C to stop\n";
    std::cout.flush();

    while (true) {
        socket_t client = accept(server_sock, nullptr, nullptr);
        if (client == INVALID_SOCK) continue;
        handleClient(client);
        CLOSE_SOCKET(client);
    }
}
