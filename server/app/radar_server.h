#pragma once
#include "query_handler.h"
#include "radar_compute.h"
#include "relative_angle.h"

// HTTP transport layer — no domain logic here.
// Deserializes JSON requests, delegates to QueryHandler, serializes JSON responses.
/**
 * HTTP REST interface for the Radar system.
 *
 * Implements a lightweight server using cpp-httplib to expose the QueryHandler 
 * logic over a JSON-based API. Handles request routing, JSON parsing/echoing,
 * and binary stream responses for high-bandwidth LUT exports.
 */
class RadarServer {
public:
    /**
     * Initializes the server with a reference to the core logic handler.
     * @param handler The QueryHandler instance (must remain valid for server life).
     * @param port    The local TCP port to listen on.
     */
    RadarServer(QueryHandler& handler, int port);

    /**
     * Starts the blocking event loop.
     * Maps the following endpoints:
     *   GET  /health     - Status check
     *   GET  /radar      - Current radar position and LUT metadata
     *   POST /radar      - Set/Update radar position (triggers LUT build)
     *   GET  /lut        - Binary download of the full elevation grid
     *   GET  /elevation  - Terrain height at specific lat/lon
     *   POST /query      - Target propagation and geometry calculation
     *   POST /convert    - UTM <-> Geodetic coordinate conversion
     */
    void start();
 // blocking; Ctrl-C to terminate

private:
    QueryHandler& handler_;
    int           port_;
};
