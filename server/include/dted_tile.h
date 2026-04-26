#pragma once
#include "constants.h"
#include <string>
#include <cstdint>

/**
 * One 1°×1° terrain tile stored in a flat 1D array.
 * Logic: column-major [lon_col][lat_row]. West edge is col 0, South edge is row 0.
 */
class DtedTile {
public:
    int    origin_lat;    // SW corner integer latitude
    int    origin_lon;    // SW corner integer longitude
    int    cols;          // Samples per line (3601 for SRTM1, 1201 for SRTM3)
    int    rows;          // Samples per column
    double post_spacing;  // Degrees between adjacent samples

    /** Raw elevation data in decimeters or meters. Static size for zero-allocation. */
    int16_t elevation[SRTM1_COLS * SRTM1_ROWS]; 

    DtedTile();

    /** Loads .dt2 (big-endian with 3428-byte header). */
    void loadDTED2(const std::string& filepath);

    /** Loads .hgt (raw big-endian, size-detected). */
    void loadSRTM(const std::string& filepath);

    /** Safe sample lookup; converts -32768 (void) to 0m. */
    double postElevation(int col, int row) const;
};
