#pragma once
#include "dem_database.h"
#include <string>

class TdpDtmTcpServer {
public:
    TdpDtmTcpServer(const std::string& tiles_dir, int port, double max_range_m,
                    DemDatabase::Format fmt = DemDatabase::Format::SRTM);
    void start();

private:
    std::string         tiles_dir_;
    int                 port_;
    double              max_range_m_;
    DemDatabase::Format dem_fmt_;

#ifdef _WIN32
    void handleClient(unsigned long long sockfd) const;
#else
    void handleClient(int sockfd) const;
#endif
};
