#pragma once
#include <string>

// TCP server that accepts LUT export requests and returns a raw int32[az][range] array
// or saves it to a binary file on the server side.
class LutTcpServer {
public:
    LutTcpServer(const std::string& tiles_dir, int port);
    void start();   // blocks until interrupted

private:
    std::string tiles_dir_;
    int         port_;

#ifdef _WIN32
    void handleClient(unsigned long long sockfd) const;
#else
    void handleClient(int sockfd) const;
#endif
};
