#include "tdp_dtm_tcp_server.h"
#include "lut_exporter.h"
#include "coord_convert.h"
#include "types.h"
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

#pragma pack(push, 1)
struct BlkHdr {
    uint32_t blk_id;
    uint32_t blk_length;
    uint32_t time_tag;
    uint32_t msg_seq_no;
    uint32_t ch_id;
};

struct HeightReqData {
    float x;  // Easting
    float y;  // Northing
    float z;  // Zone (whole number cast to int)
};

struct HeightRespData {
    int32_t validity;     // 0=ok, -1=failed
    float   dted_height;  // terrain MSL at radar position
    float   spare[3];
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

TdpDtmTcpServer::TdpDtmTcpServer(const std::string& tiles_dir, int port,
                                   double max_range_m, DemDatabase::Format fmt)
    : tiles_dir_(tiles_dir), port_(port), max_range_m_(max_range_m), dem_fmt_(fmt) {}

#ifdef _WIN32
void TdpDtmTcpServer::handleClient(unsigned long long sock) const
#else
void TdpDtmTcpServer::handleClient(int sock) const
#endif
{
    BlkHdr        hdr{};
    HeightReqData req{};
    if (!recvAll(sock, &hdr, sizeof(hdr))) return;
    if (!recvAll(sock, &req, sizeof(req))) return;

    HeightRespData resp{};
    std::memset(&resp, 0, sizeof(resp));

    try {
        UtmPoint utm{ static_cast<double>(req.x),
                      static_cast<double>(req.y),
                      static_cast<int>(req.z),
                      'N' };
        double lat_deg = 0.0, lon_deg = 0.0;
        utm_to_ll(utm, lat_deg, lon_deg);

        LLA         radar{ lat_deg, lon_deg, 0.0 };
        LutExporter exporter(radar, max_range_m_, tiles_dir_,
                             LutExporter::AltMode::AGL, dem_fmt_);
        LutExportData data = exporter.exportData();
        exporter.saveToFile(data, "output.bin");

        resp.validity    = 0;
        resp.dted_height = static_cast<float>(exporter.radarTerrainM());
    } catch (const std::exception& e) {
        std::cerr << "[tdp_dtm] error: " << e.what() << "\n";
        resp.validity    = -1;
        resp.dted_height = 0.0f;
    }

    sendAll(sock, &resp, sizeof(resp));
}

void TdpDtmTcpServer::start()
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
    std::cout << "[tdp_dtm] Listening on port " << port_ << " -- Ctrl-C to stop\n";
    std::cout.flush();

    while (true) {
        socket_t client = accept(server_sock, nullptr, nullptr);
        if (client == INVALID_SOCK) continue;
        handleClient(client);
        CLOSE_SOCKET(client);
    }
}
