#include "dted_tile.h"
#include "constants.h"
#include <fstream>
#include <stdexcept>
#include <algorithm>

DtedTile::DtedTile()
    : origin_lat(0), origin_lon(0), cols(0), rows(0), post_spacing(0.0) {}

void DtedTile::loadDTED2(const std::string& filepath)
{
    std::ifstream f(filepath, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot open DTED file: " + filepath);

    cols         = SRTM1_COLS;
    rows         = SRTM1_ROWS;
    post_spacing = SRTM1_POST_SPACING;

    static constexpr int HEADER_SIZE   = 3428;
    static constexpr int RECORD_HEADER = 8;
    static constexpr int RECORD_FOOTER = 4;

    elevation[0] = 0; // Just to avoid unused warning or similar if needed

    f.seekg(HEADER_SIZE, std::ios::beg);
    uint8_t line_buf[SRTM1_ROWS * 2];
    for (int col = 0; col < cols; ++col) {
        f.seekg(RECORD_HEADER, std::ios::cur);
        f.read(reinterpret_cast<char*>(line_buf), rows * 2);
        for (int row = 0; row < rows; ++row) {
            // Big-endian int16
            elevation[col * rows + row] =
                static_cast<int16_t>((line_buf[row * 2] << 8) | line_buf[row * 2 + 1]);
        }
        f.seekg(RECORD_FOOTER, std::ios::cur);
    }
}

void DtedTile::loadSRTM(const std::string& filepath)
{
    std::ifstream f(filepath, std::ios::binary | std::ios::ate);
    if (!f) throw std::runtime_error("Cannot open SRTM file: " + filepath);

    const std::streamsize file_size = f.tellg();
    f.seekg(0, std::ios::beg);

    if (file_size == static_cast<std::streamsize>(SRTM1_COLS) * SRTM1_ROWS * 2) {
        cols = SRTM1_COLS; rows = SRTM1_ROWS; post_spacing = SRTM1_POST_SPACING;
    } else if (file_size == static_cast<std::streamsize>(SRTM3_COLS) * SRTM3_ROWS * 2) {
        cols = SRTM3_COLS; rows = SRTM3_ROWS; post_spacing = SRTM3_POST_SPACING;
    } else {
        throw std::runtime_error("Unknown SRTM file size (" +
            std::to_string(file_size) + " bytes): " + filepath);
    }

    // HGT is row-major, north-to-south, big-endian int16.
    // Read line by line to avoid large temporary allocation of the whole file.
    uint8_t row_buf[SRTM1_COLS * 2];
    for (int r = 0; r < rows; ++r) {
        f.read(reinterpret_cast<char*>(row_buf), cols * 2);
        const int south_row = rows - 1 - r;  // flip N->S to S->N
        for (int c = 0; c < cols; ++c) {
            const int raw_idx = c * 2;
            // Column-major layout: elevation[col * rows + row].
            elevation[c * rows + south_row] =
                static_cast<int16_t>((row_buf[raw_idx] << 8) | row_buf[raw_idx + 1]);
        }
    }
}

double DtedTile::postElevation(int col, int row) const
{
    // Bicubic sampling reaches ±1 post outside the query cell; clamp to tile edge.
    col = std::clamp(col, 0, cols - 1);
    row = std::clamp(row, 0, rows - 1);
    int16_t v = elevation[col * rows + row];
    return (v == -32768) ? 0.0 : static_cast<double>(v); // -32768 = SRTM void (sea/no-data)
}
