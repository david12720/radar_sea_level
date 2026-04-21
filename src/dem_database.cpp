#include "dem_database.h"
#include "constants.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <stdexcept>

// ── Bicubic interpolation (Catmull-Rom kernel) ────────────────────────────────
//
// Catmull-Rom piecewise cubic: C1-continuous, passes through the sampled posts.
// Two segments based on distance |t| from the center post:
//   |t| < 1 (inner): smoothly blends the two nearest posts
//   |t| < 2 (outer): tapers off the two flanking posts
static double cubicWeight(double t)
{
    t = std::abs(t);
    if (t < 1.0) return  1.5*t*t*t - 2.5*t*t + 1.0;
    if (t < 2.0) return -0.5*t*t*t + 2.5*t*t - 4.0*t + 2.0;
    return 0.0;
}

// Evaluates elevation at fractional post coordinates (frac_col, frac_row)
// using a 4×4 neighborhood of posts. Weights are separable: w(col) * w(row).
static double bicubicInterpolate(const DtedTile& tile, double frac_col, double frac_row)
{
    int c0 = static_cast<int>(std::floor(frac_col)); // integer post to the left
    int r0 = static_cast<int>(std::floor(frac_row)); // integer post below
    double dc = frac_col - c0; // fractional offset within the cell, [0,1)
    double dr = frac_row - r0;

    double result = 0.0;
    for (int ci = -1; ci <= 2; ++ci) {
        double wc = cubicWeight(ci - dc);
        for (int ri = -1; ri <= 2; ++ri)
            result += wc * cubicWeight(ri - dr) * tile.postElevation(c0 + ci, r0 + ri);
    }
    return result;
}

// ── DemDatabase ───────────────────────────────────────────────────────────────

void DemDatabase::loadTile(const std::string& filepath, int origin_lat, int origin_lon, Format fmt)
{
    DtedTile tile;
    tile.origin_lat = origin_lat;
    tile.origin_lon = origin_lon;

    if (fmt == Format::DTED2)
        tile.loadDTED2(filepath);
    else
        tile.loadSRTM(filepath);

    tiles_[tileKey(origin_lat, origin_lon)] = std::move(tile);
    std::cout << "[DEM] Loaded tile (" << origin_lat << ", " << origin_lon
              << ") from: " << filepath << "\n";
}

double DemDatabase::getElevation(double lat_deg, double lon_deg) const
{
    int tile_lat = static_cast<int>(std::floor(lat_deg));
    int tile_lon = static_cast<int>(std::floor(lon_deg));

    auto it = tiles_.find(tileKey(tile_lat, tile_lon));
    if (it == tiles_.end())
        throw std::runtime_error("No DEM tile loaded for lat=" +
            std::to_string(tile_lat) + " lon=" + std::to_string(tile_lon));

    const DtedTile& tile = it->second;
    // Convert geographic coords to fractional post indices within this tile.
    double frac_col = (lon_deg - tile.origin_lon) / tile.post_spacing;
    double frac_row = (lat_deg - tile.origin_lat) / tile.post_spacing;
    return bicubicInterpolate(tile, frac_col, frac_row);
}

bool DemDatabase::hasTile(double lat_deg, double lon_deg) const
{
    return tiles_.count(tileKey(
        static_cast<int>(std::floor(lat_deg)),
        static_cast<int>(std::floor(lon_deg)))) > 0;
}

std::string DemDatabase::dted2Filename(int origin_lat, int origin_lon)
{
    std::ostringstream oss;
    oss << (origin_lon >= 0 ? 'e' : 'w')
        << std::setfill('0') << std::setw(3) << std::abs(origin_lon)
        << (origin_lat >= 0 ? 'n' : 's')
        << std::setfill('0') << std::setw(2) << std::abs(origin_lat)
        << ".dt2";
    return oss.str();
}

std::string DemDatabase::srtmFilename(int origin_lat, int origin_lon)
{
    std::ostringstream oss;
    oss << (origin_lat >= 0 ? 'N' : 'S')
        << std::setfill('0') << std::setw(2) << std::abs(origin_lat)
        << (origin_lon >= 0 ? 'E' : 'W')
        << std::setfill('0') << std::setw(3) << std::abs(origin_lon)
        << ".hgt";
    return oss.str();
}

std::string DemDatabase::tileKey(int lat, int lon)
{
    return std::to_string(lat) + "_" + std::to_string(lon);
}

int DemDatabase::loadTilesAround(const LLA& radar, double max_range_m,
                                  const std::string& tiles_dir, Format fmt)
{
    // Degrees of latitude spanned by max_range (constant — 1° lat ~ 111 km).
    const double lat_span = (max_range_m / EARTH_RADIUS) * RAD2DEG;
    // Longitude span shrinks toward the poles: 1° lon = 111 km * cos(lat).
    const double lon_span = (max_range_m / (EARTH_RADIUS * std::cos(radar.lat_deg * DEG2RAD))) * RAD2DEG;

    // Integer SW corners of all 1°×1° cells overlapping the bounding box.
    const int lat_min = static_cast<int>(std::floor(radar.lat_deg - lat_span));
    const int lat_max = static_cast<int>(std::floor(radar.lat_deg + lat_span));
    const int lon_min = static_cast<int>(std::floor(radar.lon_deg - lon_span));
    const int lon_max = static_cast<int>(std::floor(radar.lon_deg + lon_span));

    int loaded = 0;
    for (int lat = lat_min; lat <= lat_max; ++lat) {
        for (int lon = lon_min; lon <= lon_max; ++lon) {
            std::string fname = (fmt == Format::DTED2)
                                ? dted2Filename(lat, lon)
                                : srtmFilename(lat, lon);
            std::string fpath = tiles_dir + fname;
            if (std::ifstream(fpath).good()) {
                loadTile(fpath, lat, lon, fmt);
                ++loaded;
            }
        }
    }
    return loaded;
}
