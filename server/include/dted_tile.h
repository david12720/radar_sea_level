#pragma once
#include <string>
#include <vector>
#include <cstdint>

// One 1°×1° DTED2 or SRTM tile stored column-major [lon_col][lat_row].
// col 0 = west edge, row 0 = south edge.
class DtedTile {
public:
    int    origin_lat;    // SW corner integer latitude  (e.g. 31 for N31)
    int    origin_lon;    // SW corner integer longitude (e.g. 34 for E034)
    int    cols;          // longitude posts: 3601 (SRTM1) or 1201 (SRTM3)
    int    rows;          // latitude posts:  3601 (SRTM1) or 1201 (SRTM3)
    double post_spacing;  // degrees between adjacent posts

    std::vector<std::vector<int16_t>> elevation; // [col][row]

    DtedTile();

    // Load DTED Level-2 (.dt2) — 3601×3601, big-endian int16, 3428-byte header
    void loadDTED2(const std::string& filepath);

    // Load SRTM HGT — auto-detects SRTM1 vs SRTM3 from file size
    void loadSRTM(const std::string& filepath);

    // Clamped post lookup; returns 0 for void (-32768) cells
    double postElevation(int col, int row) const;
};
