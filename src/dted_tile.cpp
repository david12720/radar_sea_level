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

    static constexpr int HEADER_SIZE    = 3428;
    static constexpr int RECORD_HEADER  = 4;
    static constexpr int RECORD_FOOTER  = 4;

    f.seekg(HEADER_SIZE, std::ios::beg);
    elevation.assign(cols, std::vector<int16_t>(rows, 0));

    for (int col = 0; col < cols; ++col) {
        f.seekg(RECORD_HEADER, std::ios::cur);
        for (int row = 0; row < rows; ++row) {
            uint8_t hi, lo;
            f.read(reinterpret_cast<char*>(&hi), 1);
            f.read(reinterpret_cast<char*>(&lo), 1);
            elevation[col][row] = static_cast<int16_t>((hi << 8) | lo);
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

    // Read row-major N→S, then transpose to col-major S→N
    std::vector<std::vector<int16_t>> tmp(rows, std::vector<int16_t>(cols));
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            uint8_t hi, lo;
            f.read(reinterpret_cast<char*>(&hi), 1);
            f.read(reinterpret_cast<char*>(&lo), 1);
            tmp[r][c] = static_cast<int16_t>((hi << 8) | lo);
        }
    }

    elevation.assign(cols, std::vector<int16_t>(rows, 0));
    for (int col = 0; col < cols; ++col)
        for (int row = 0; row < rows; ++row)
            elevation[col][row] = tmp[rows - 1 - row][col]; // flip N↔S
}

double DtedTile::postElevation(int col, int row) const
{
    col = std::clamp(col, 0, cols - 1);
    row = std::clamp(row, 0, rows - 1);
    int16_t v = elevation[col][row];
    return (v == -32768) ? 0.0 : static_cast<double>(v);
}
