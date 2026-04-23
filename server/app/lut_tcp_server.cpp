#include "lut_tcp_server.h"
#include "lut_exporter.h"
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

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

// ── Wire protocol ────────────────────────────────────────────────────────────
//
// Request (little-endian, fixed header 33 bytes):
//   double  lat_deg
//   double  lon_deg
//   double  alt_agl_m    (height above local ground; server resolves MSL from DEM)
//   double  max_range_m
//   uint8   mode         (0 = send data over TCP, 1 = save to file)
//
//   [mode == 1 only]
//   uint16  filename_len
//   char[]  filename (filename_len bytes, no null terminator)
//
// Response:
//   mode 0:  uint32 az_count, uint32 range_count, int32[az_count * range_count]
//   mode 1:  uint8  status   (0 = ok, 1 = error)

#pragma pack(push, 1)
struct RequestHeader {
    double  lat_deg;
    double  lon_deg;
    double  alt_agl_m;
    double  max_range_m;
    uint8_t mode;
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

// ─────────────────────────────────────────────────────────────────────────────

LutTcpServer::LutTcpServer(const std::string& tiles_dir, int port)
    : tiles_dir_(tiles_dir), port_(port) {}

#ifdef _WIN32
void LutTcpServer::handleClient(unsigned long long sock) const
#else
void LutTcpServer::handleClient(int sock) const
#endif
{
    RequestHeader hdr{};
    if (!recvAll(sock, &hdr, sizeof(hdr))) return;

    std::string filename;
    if (hdr.mode == 1) {
        uint16_t name_len = 0;
        if (!recvAll(sock, &name_len, sizeof(name_len))) return;
        filename.resize(name_len);
        if (name_len > 0 && !recvAll(sock, filename.data(), name_len)) return;
    }

    LLA radar { hdr.lat_deg, hdr.lon_deg, hdr.alt_agl_m };

    try {
        LutExporter   exporter(radar, hdr.max_range_m, tiles_dir_);
        LutExportData data = exporter.exportData();

        if (hdr.mode == 0) {
            uint32_t az    = data.az_count;
            uint32_t range = data.range_count;
            if (!sendAll(sock, &az,    sizeof(az)))    return;
            if (!sendAll(sock, &range, sizeof(range))) return;
            sendAll(sock, data.cells,
                    data.total_cells * sizeof(int32_t));
        } else {
            exporter.saveToFile(data, filename);
            uint8_t status = 0;
            sendAll(sock, &status, sizeof(status));
        }
    } catch (const std::exception& e) {
        std::cerr << "[lut_tcp] error: " << e.what() << "\n";
        if (hdr.mode == 1) {
            uint8_t status = 1;
            sendAll(sock, &status, sizeof(status));
        }
    }
}

void LutTcpServer::start()
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
    std::cout << "[lut_tcp] Listening on port " << port_ << " -- Ctrl-C to stop\n";
    std::cout.flush();

    while (true) {
        socket_t client = accept(server_sock, nullptr, nullptr);
        if (client == INVALID_SOCK) continue;
        handleClient(client);
        CLOSE_SOCKET(client);
    }
}
