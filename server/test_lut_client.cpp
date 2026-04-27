/**
 * TCP client for testing lut_server in C++.
 * Mirrors the logic of test_lut_client.py.
 */

#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <cstdint>
#include <iomanip>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <netdb.h>
#endif

// Fixed header structure (33 bytes)
#pragma pack(push, 1)
struct LutRequestHeader {
    double  lat;
    double  lon;
    double  alt;
    double  max_range;
    uint8_t mode; // 0 = receive, 1 = server-side save
};
#pragma pack(pop)

void cleanup_sockets() {
#ifdef _WIN32
    WSACleanup();
#endif
}

bool recv_all(int sock, char* buffer, size_t length) {
    size_t total_received = 0;
    while (total_received < length) {
        int received = recv(sock, buffer + total_received, (int)(length - total_received), 0);
        if (received <= 0) return false;
        total_received += received;
    }
    return true;
}

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    int port = 9000;
    double lat = 31.860342;
    double lon = 34.919733;
    double alt_agl = 0.0;
    double max_range = 15000.0;
    std::string save_path = "";

    // Simple arg parsing
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--host" && i + 1 < argc) host = argv[++i];
        else if (arg == "--port" && i + 1 < argc) port = std::stoi(argv[++i]);
        else if (arg == "--lat" && i + 1 < argc) lat = std::stod(argv[++i]);
        else if (arg == "--lon" && i + 1 < argc) lon = std::stod(argv[++i]);
        else if (arg == "--alt-agl" && i + 1 < argc) alt_agl = std::stod(argv[++i]);
        else if (arg == "--max-range" && i + 1 < argc) max_range = std::stod(argv[++i]);
        else if (arg == "--save" && i + 1 < argc) save_path = argv[++i];
    }

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }
#endif

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Socket creation failed\n";
        cleanup_sockets();
        return 1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr);

    std::cout << "Connecting to " << host << ":" << port << " ...\n";
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Connection failed\n";
        cleanup_sockets();
        return 1;
    }

    LutRequestHeader header{ lat, lon, alt_agl, max_range, (uint8_t)(save_path.empty() ? 0 : 1) };
    send(sock, (const char*)&header, sizeof(header), 0);

    if (header.mode == 1) {
        uint16_t path_len = (uint16_t)save_path.length();
        send(sock, (const char*)&path_len, sizeof(path_len), 0);
        send(sock, save_path.c_str(), path_len, 0);
    }

    std::cout << "Request sent. Mode: " << (header.mode == 1 ? "save" : "receive") << "\n";

    if (header.mode == 0) {
        uint32_t az_count, range_count;
        if (!recv_all(sock, (char*)&az_count, 4) || !recv_all(sock, (char*)&range_count, 4)) {
            std::cerr << "Failed to receive dimensions\n";
        } else {
            std::cout << "Array shape: int32[" << az_count << "][" << range_count << "]\n";
            size_t total_cells = (size_t)az_count * range_count;
            std::vector<int32_t> cells(total_cells);
            
            std::cout << "Receiving " << total_cells << " cells (" << (total_cells * 4.0 / 1024 / 1024) << " MB) ...\n";
            
            bool success = true;
            for (size_t i = 0; i < total_cells; ++i) {
                int32_t height_mtr;
                if (!recv_all(sock, (char*)&height_mtr, sizeof(int32_t))) {
                    success = false;
                    break;
                }
                cells[i] = height_mtr;
            }

            if (success) {
                std::cout << "\n── Spot checks ─────────────────────────────────────────────\n";
                auto check = [&](int ai, int ri, const char* label) {
                    if (ai < (int)az_count && ri < (int)range_count) {
                        int32_t val = cells[ai * range_count + ri];
                        std::cout << "  [" << std::setw(4) << ai << "][" << std::setw(4) << ri << "]  " 
                                  << std::left << std::setw(45) << label << " elev=" << val << " m\n";
                    }
                };

                check(0, 0, "az=0.0°  range=0m    (radar position)");
                check(12, 100, "az=1.2°  range=1500m");
                check(900, 200, "az=90.0° range=3000m (East)");
                check(az_count - 1, range_count - 1, "last cell");

                int32_t min_e = cells[0], max_e = cells[0];
                size_t non_zero = 0;
                for (auto v : cells) {
                    if (v < min_e) min_e = v;
                    if (v > max_e) max_e = v;
                    if (v != 0) non_zero++;
                }
                std::cout << "\n── Statistics ──────────────────────────────────────────────\n";
                std::cout << "  Min elev : " << min_e << " m\n";
                std::cout << "  Max elev : " << max_e << " m\n";
                std::cout << "  Non-zero : " << non_zero << " / " << total_cells << " (" << (100.0 * non_zero / total_cells) << "%)\n";
                std::cout << "\nOK\n";
            } else {
                std::cerr << "Failed to receive data\n";
            }
        }
    } else {
        uint8_t status;
        if (recv_all(sock, (char*)&status, 1)) {
            if (status == 0) std::cout << "Server saved file: " << save_path << " OK\n";
            else std::cerr << "Server reported error saving file\n";
        }
    }

#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
    cleanup_sockets();
    return 0;
}
