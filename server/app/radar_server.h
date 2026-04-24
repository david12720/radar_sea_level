#pragma once
#include "query_handler.h"
#include "radar_compute.h"
#include "relative_angle.h"

// HTTP transport layer — no domain logic here.
// Deserializes JSON requests, delegates to QueryHandler, serializes JSON responses.
class RadarServer {
public:
    RadarServer(QueryHandler& handler, int port);
    void start(); // blocking; Ctrl-C to terminate

private:
    QueryHandler& handler_;
    int           port_;
};
