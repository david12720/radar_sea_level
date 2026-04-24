#pragma once
#include <string>

// TCP server for high-rate target queries.
//
// Wire protocol (all values little-endian):
//
// Session setup (once per connection, 24 bytes):
//   double  radar_lat_deg
//   double  radar_lon_deg
//   double  radar_alt_msl_m
//
// Per target (32 bytes in, 40 bytes out, repeated until disconnect):
//   In:  double range_m, azimuth_deg, elevation_deg, ground_elevation_m
//   Out: double lat_deg, lon_deg, alt_msl_m, agl_m, relative_elev_deg
class TargetTcpServer {
public:
    explicit TargetTcpServer(int port);
    void start();   // blocks until interrupted

private:
    int port_;

#ifdef _WIN32
    void handleClient(unsigned long long sockfd) const;
#else
    void handleClient(int sockfd) const;
#endif
};
