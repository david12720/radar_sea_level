/**
 * radar_sea_level.cpp
 *
 * Computes the ground elevation (sea level) beneath a radar target using:
 *   - Flat-Earth geometry to find target Lat/Lon from range/az/el
 *   - DTED Level 2 (.dt2) or SRTM (.hgt) binary tile parsing
 *   - Bicubic interpolation for smooth sub-post elevation accuracy
 *
 * DTED2 tile naming: e034n31.dt2  → 1° × 1° cell, 3601×3601 posts @ ~30m
 * SRTM   tile naming: N31E034.hgt → same layout, same binary format
 *
 * Binary layout (both formats, big-endian int16):
 *   File = columns of longitude, each column = rows of latitude (S→N)
 *   post[lon_col][lat_row] stored column-major
 */

#include <iostream>
#include <fstream>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstdint>

// ─────────────────────────────────────────────
// Constants
// ─────────────────────────────────────────────
static constexpr double DEG2RAD      = M_PI / 180.0;
static constexpr double RAD2DEG      = 180.0 / M_PI;
static constexpr double EARTH_RADIUS = 6'371'000.0; // meters

// DTED2 / SRTM HGT Level-2 (1 arc-second, ~30m)
static constexpr int    SRTM1_COLS         = 3601;
static constexpr int    SRTM1_ROWS         = 3601;
static constexpr double SRTM1_POST_SPACING = 1.0 / 3600.0;

// SRTM3 HGT (3 arc-second, ~90m) — viewfinderpanoramas.org
static constexpr int    SRTM3_COLS         = 1201;
static constexpr int    SRTM3_ROWS         = 1201;
static constexpr double SRTM3_POST_SPACING = 1.0 / 1200.0;

// ─────────────────────────────────────────────
// Data structures
// ─────────────────────────────────────────────
struct LLA {
    double lat_deg;
    double lon_deg;
    double alt_m;
};

struct RadarMeasurement {
    double range_m;
    double azimuth_deg;    // True North clockwise
    double elevation_deg;  // Positive up from horizon
};

struct TargetResult {
    LLA    position;               // Target Lat/Lon/Alt MSL
    double ground_elevation_m;     // Terrain height MSL at target Lat/Lon
    double target_height_agl_m;    // Target altitude above ground level
    double horizontal_range_m;
    double vertical_offset_m;
};

// ─────────────────────────────────────────────
// DTED Tile
// ─────────────────────────────────────────────
/**
 * Represents one 1°×1° DTED2 or SRTM tile.
 * Origin = SW corner (floor of lat, floor of lon).
 *
 * Storage: column-major [lon_col 0..3600][lat_row 0..3600]
 *   col 0 = west edge, col 3600 = east edge
 *   row 0 = south edge, row 3600 = north edge
 */
class DtedTile {
public:
    int    origin_lat;   // SW corner integer latitude  (e.g. 31 for N31)
    int    origin_lon;   // SW corner integer longitude (e.g. 34 for E034)
    int    cols;         // number of longitude posts (3601 or 1201)
    int    rows;         // number of latitude posts  (3601 or 1201)
    double post_spacing; // degrees between posts

    // elevation[col][row], col=lon axis, row=lat axis
    std::vector<std::vector<int16_t>> elevation;

    DtedTile() : origin_lat(0), origin_lon(0), cols(0), rows(0), post_spacing(0.0) {}

    /**
     * Load from a DTED2 (.dt2) or SRTM (.hgt) file.
     *
     * DTED2: has a 3428-byte header before data; each data record is:
     *   sentinel(1) + block_count(3) + data(3601×2 bytes) + checksum(4) = 3612 bytes per column
     *
     * SRTM HGT: pure big-endian int16 array, row-major (N→S), 3601×3601
     *   → we re-index to column-major (S→N) internally
     */
    void loadDTED2(const std::string& filepath) {
        std::ifstream f(filepath, std::ios::binary);
        if (!f) throw std::runtime_error("Cannot open DTED file: " + filepath);

        cols         = SRTM1_COLS;
        rows         = SRTM1_ROWS;
        post_spacing = SRTM1_POST_SPACING;

        static constexpr int DTED2_HEADER_SIZE = 3428;
        f.seekg(DTED2_HEADER_SIZE, std::ios::beg);

        elevation.assign(cols, std::vector<int16_t>(rows, 0));

        static constexpr int RECORD_HEADER = 4;
        static constexpr int RECORD_FOOTER = 4;

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

    // Auto-detects SRTM1 (3601×3601) vs SRTM3 (1201×1201) from file size.
    void loadSRTM(const std::string& filepath) {
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

        // SRTM is row-major North→South; read then transpose to col-major South→North
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
        for (int col = 0; col < cols; ++col) {
            for (int row = 0; row < rows; ++row) {
                elevation[col][row] = tmp[rows - 1 - row][col]; // flip N↔S
            }
        }
    }

    double postElevation(int col, int row) const {
        col = std::clamp(col, 0, cols - 1);
        row = std::clamp(row, 0, rows - 1);
        int16_t v = elevation[col][row];
        return (v == -32768) ? 0.0 : static_cast<double>(v);
    }
};

// ─────────────────────────────────────────────
// Bicubic interpolation
// ─────────────────────────────────────────────
/**
 * Cubic kernel (Catmull-Rom) weight for distance t in [-2, 2].
 */
static double cubicWeight(double t) {
    t = std::abs(t);
    if (t < 1.0) return 1.5*t*t*t - 2.5*t*t + 1.0;
    if (t < 2.0) return -0.5*t*t*t + 2.5*t*t - 4.0*t + 2.0;
    return 0.0;
}

// frac_col and frac_row are fractional grid coordinates within the tile.
static double bicubicInterpolate(const DtedTile& tile, double frac_col, double frac_row) {
    int c0 = static_cast<int>(std::floor(frac_col));
    int r0 = static_cast<int>(std::floor(frac_row));
    double dc = frac_col - c0;
    double dr = frac_row - r0;

    double result = 0.0;
    for (int ci = -1; ci <= 2; ++ci) {
        double wc = cubicWeight(ci - dc);
        for (int ri = -1; ri <= 2; ++ri) {
            double wr = cubicWeight(ri - dr);
            result += wc * wr * tile.postElevation(c0 + ci, r0 + ri);
        }
    }
    return result;
}

// ─────────────────────────────────────────────
// DEM Database — manages multiple tiles
// ─────────────────────────────────────────────
class DemDatabase {
public:
    enum class Format { DTED2, SRTM };

    /**
     * Add a tile to the database by loading it from disk.
     * origin_lat/lon = SW integer corner of the 1°×1° cell.
     */
    void loadTile(const std::string& filepath, int origin_lat, int origin_lon, Format fmt) {
        std::string key = tileKey(origin_lat, origin_lon);
        DtedTile tile;
        tile.origin_lat = origin_lat;
        tile.origin_lon = origin_lon;

        if (fmt == Format::DTED2)
            tile.loadDTED2(filepath);
        else
            tile.loadSRTM(filepath);

        tiles_[key] = std::move(tile);
        std::cout << "[DEM] Loaded tile (" << origin_lat << ", " << origin_lon
                  << ") from: " << filepath << "\n";
    }

    /**
     * Query ground elevation at an arbitrary Lat/Lon using bicubic interpolation.
     * Throws if no tile covers the requested location.
     */
    double getElevation(double lat_deg, double lon_deg) const {
        int tile_lat = static_cast<int>(std::floor(lat_deg));
        int tile_lon = static_cast<int>(std::floor(lon_deg));

        std::string key = tileKey(tile_lat, tile_lon);
        auto it = tiles_.find(key);
        if (it == tiles_.end()) {
            throw std::runtime_error(
                "No DEM tile loaded for lat=" + std::to_string(tile_lat) +
                " lon=" + std::to_string(tile_lon));
        }

        const DtedTile& tile = it->second;

        double frac_col = (lon_deg - tile.origin_lon) / tile.post_spacing;
        double frac_row = (lat_deg - tile.origin_lat) / tile.post_spacing;

        return bicubicInterpolate(tile, frac_col, frac_row);
    }

    bool hasTile(double lat_deg, double lon_deg) const {
        int tile_lat = static_cast<int>(std::floor(lat_deg));
        int tile_lon = static_cast<int>(std::floor(lon_deg));
        return tiles_.count(tileKey(tile_lat, tile_lon)) > 0;
    }

    /**
     * Generate the standard DTED2 filename for a given SW corner.
     * Example: origin_lat=31, origin_lon=34 → "e034n31.dt2"
     */
    static std::string dted2Filename(int origin_lat, int origin_lon) {
        std::ostringstream oss;
        oss << (origin_lon >= 0 ? 'e' : 'w')
            << std::setfill('0') << std::setw(3) << std::abs(origin_lon)
            << (origin_lat >= 0 ? 'n' : 's')
            << std::setfill('0') << std::setw(2) << std::abs(origin_lat)
            << ".dt2";
        return oss.str();
    }

    /**
     * Generate the standard SRTM HGT filename for a given SW corner.
     * Example: origin_lat=31, origin_lon=34 → "N31E034.hgt"
     */
    static std::string srtmFilename(int origin_lat, int origin_lon) {
        std::ostringstream oss;
        oss << (origin_lat >= 0 ? 'N' : 'S')
            << std::setfill('0') << std::setw(2) << std::abs(origin_lat)
            << (origin_lon >= 0 ? 'E' : 'W')
            << std::setfill('0') << std::setw(3) << std::abs(origin_lon)
            << ".hgt";
        return oss.str();
    }

private:
    std::unordered_map<std::string, DtedTile> tiles_;

    static std::string tileKey(int lat, int lon) {
        return std::to_string(lat) + "_" + std::to_string(lon);
    }
};

// ─────────────────────────────────────────────
// Geometry helpers (shared by DEM query and LUT build)
// ─────────────────────────────────────────────
struct GroundPoint {
    double lat_deg;
    double lon_deg;
};

static GroundPoint groundPoint(const LLA& radar, double horiz_range_m, double azimuth_deg)
{
    const double az_rad      = azimuth_deg * DEG2RAD;
    const double radar_lat_r = radar.lat_deg * DEG2RAD;
    return {
        radar.lat_deg + (horiz_range_m * std::cos(az_rad) / EARTH_RADIUS) * RAD2DEG,
        radar.lon_deg + (horiz_range_m * std::sin(az_rad) / (EARTH_RADIUS * std::cos(radar_lat_r))) * RAD2DEG
    };
}

// ─────────────────────────────────────────────
// Pre-computed elevation lookup table
// Indexed by [range_idx][azimuth_idx] where:
//   range_idx    = floor(horiz_range_m / range_step_m)
//   azimuth_idx  = round(azimuth_deg  / az_step_deg) % num_azimuths
// ─────────────────────────────────────────────
struct LutConfig {
    double max_range_m  = 50000.0;  // meters
    double range_step_m = 15.0;     // meters
    double az_step_deg  = 0.1;      // degrees
};

class ElevationLUT {
public:
    void build(const LLA& radar, const DemDatabase& dem, const LutConfig& cfg)
    {
        cfg_         = cfg;
        radar_       = radar;
        num_ranges_  = static_cast<int>(std::ceil(cfg.max_range_m / cfg.range_step_m)) + 1;
        num_azimuths_= static_cast<int>(std::round(360.0 / cfg.az_step_deg));

        table_.assign(num_ranges_, std::vector<float>(num_azimuths_, 0.0f));

        int skipped = 0;
        for (int ri = 0; ri < num_ranges_; ++ri) {
            const double horiz_range = ri * cfg.range_step_m;
            for (int ai = 0; ai < num_azimuths_; ++ai) {
                const double azimuth = ai * cfg.az_step_deg;
                GroundPoint gp = groundPoint(radar, horiz_range, azimuth);
                if (dem.hasTile(gp.lat_deg, gp.lon_deg)) {
                    table_[ri][ai] = static_cast<float>(dem.getElevation(gp.lat_deg, gp.lon_deg));
                } else {
                    table_[ri][ai] = 0.0f; // no tile → assume sea level
                    ++skipped;
                }
            }
        }

        std::cout << "[LUT] Built " << num_ranges_ << " × " << num_azimuths_
                  << " table (" << (num_ranges_ * num_azimuths_ * 4 / 1024 / 1024) << " MB)"
                  << ", " << skipped << " cells defaulted to 0 (no tile)\n";
    }

    // Returns ground elevation for a given horizontal range and azimuth.
    double lookup(double horiz_range_m, double azimuth_deg) const
    {
        int ri = static_cast<int>(horiz_range_m / cfg_.range_step_m + 0.5);
        ri = std::clamp(ri, 0, num_ranges_ - 1);

        // Normalize azimuth to [0, 360)
        azimuth_deg = std::fmod(azimuth_deg, 360.0);
        if (azimuth_deg < 0.0) azimuth_deg += 360.0;
        int ai = static_cast<int>(azimuth_deg / cfg_.az_step_deg + 0.5) % num_azimuths_;

        return static_cast<double>(table_[ri][ai]);
    }

    int numRanges()   const { return num_ranges_;   }
    int numAzimuths() const { return num_azimuths_; }
    const LutConfig& config() const { return cfg_; }

private:
    LutConfig                          cfg_;
    LLA                                radar_;
    int                                num_ranges_   = 0;
    int                                num_azimuths_ = 0;
    std::vector<std::vector<float>>    table_;
};

// ─────────────────────────────────────────────
// Core radar computation
// ─────────────────────────────────────────────
static TargetResult computeGeometry(const LLA& radar, const RadarMeasurement& meas)
{
    const double el_rad      = meas.elevation_deg * DEG2RAD;
    const double horiz_range = meas.range_m * std::cos(el_rad);
    const double vert_offset = meas.range_m * std::sin(el_rad);
    GroundPoint gp           = groundPoint(radar, horiz_range, meas.azimuth_deg);

    TargetResult res;
    res.horizontal_range_m = horiz_range;
    res.vertical_offset_m  = vert_offset;
    res.position           = { gp.lat_deg, gp.lon_deg, radar.alt_m + vert_offset };
    return res;
}

TargetResult computeTargetSeaLevel(const LLA& radar,
                                   const RadarMeasurement& meas,
                                   const DemDatabase& dem)
{
    TargetResult res          = computeGeometry(radar, meas);
    res.ground_elevation_m    = dem.getElevation(res.position.lat_deg, res.position.lon_deg);
    res.target_height_agl_m   = res.position.alt_m - res.ground_elevation_m;
    return res;
}

TargetResult computeTargetSeaLevel(const LLA& radar,
                                   const RadarMeasurement& meas,
                                   const ElevationLUT& lut)
{
    TargetResult res          = computeGeometry(radar, meas);
    res.ground_elevation_m    = lut.lookup(res.horizontal_range_m, meas.azimuth_deg);
    res.target_height_agl_m   = res.position.alt_m - res.ground_elevation_m;
    return res;
}

// ─────────────────────────────────────────────
// Utility: pretty print
// ─────────────────────────────────────────────
void printResult(const LLA& radar, const RadarMeasurement& meas, const TargetResult& res)
{
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "======================================\n";
    std::cout << "  RADAR\n";
    std::cout << "    Lat / Lon  : " << radar.lat_deg << " / " << radar.lon_deg << " deg\n";
    std::cout << "    Altitude   : " << radar.alt_m << " m MSL\n";
    std::cout << "--------------------------------------\n";
    std::cout << "  MEASUREMENT\n";
    std::cout << "    Range      : " << meas.range_m       << " m\n";
    std::cout << "    Azimuth    : " << meas.azimuth_deg   << " deg\n";
    std::cout << "    Elevation  : " << meas.elevation_deg << " deg\n";
    std::cout << "--------------------------------------\n";
    std::cout << "  TARGET\n";
    std::cout << "    Lat / Lon        : " << res.position.lat_deg << " / " << res.position.lon_deg << " deg\n";
    std::cout << "    Altitude MSL     : " << res.position.alt_m       << " m\n";
    std::cout << "    Ground Elev MSL  : " << res.ground_elevation_m   << " m  ← from DEM\n";
    std::cout << "    Height AGL       : " << res.target_height_agl_m  << " m\n";
    std::cout << "    Horiz Range      : " << res.horizontal_range_m   << " m\n";
    std::cout << "======================================\n";
}

// ─────────────────────────────────────────────
// Synthetic DEM tile for unit testing
// (generates realistic terrain without a real file)
// ─────────────────────────────────────────────
DtedTile makeSyntheticTile(int origin_lat, int origin_lon)
{
    DtedTile tile;
    tile.origin_lat  = origin_lat;
    tile.origin_lon  = origin_lon;
    tile.cols        = SRTM3_COLS;
    tile.rows        = SRTM3_ROWS;
    tile.post_spacing = SRTM3_POST_SPACING;
    tile.elevation.assign(tile.cols, std::vector<int16_t>(tile.rows, 0));

    for (int col = 0; col < tile.cols; ++col) {
        for (int row = 0; row < tile.rows; ++row) {
            double lat_frac = static_cast<double>(row) / tile.rows;
            double lon_frac = static_cast<double>(col) / tile.cols;
            double base = 50.0 + 200.0 * lat_frac;
            double hill = 80.0 * std::exp(-50.0 * (std::pow(lat_frac - 0.5, 2) +
                                                     std::pow(lon_frac - 0.5, 2)));
            tile.elevation[col][row] = static_cast<int16_t>(base + hill);
        }
    }
    return tile;
}

// ─────────────────────────────────────────────
// Main — demo
// ─────────────────────────────────────────────
int main()
{
    DemDatabase dem;
    const std::string tiles_dir = "./tiles/";

    // Load all Israel tiles (SRTM3, auto-detected from file size)
    const int lat_min = 28, lat_max = 33;
    const int lon_min = 34, lon_max = 35;

    for (int lat = lat_min; lat <= lat_max; ++lat) {
        for (int lon = lon_min; lon <= lon_max; ++lon) {
            std::string fname = DemDatabase::srtmFilename(lat, lon);
            std::string fpath = tiles_dir + fname;
            std::ifstream test(fpath);
            if (test.good()) {
                dem.loadTile(fpath, lat, lon, DemDatabase::Format::SRTM);
            }
        }
    }

    // Radar on the Tel Aviv coast, 10 m altitude
    LLA radar { 32.08, 34.76, 10.0 };

    // Build LUT (done once at startup)
    LutConfig cfg;
    cfg.max_range_m  = 50000.0; // 50 km
    cfg.range_step_m = 15.0;    // 15 m steps
    cfg.az_step_deg  = 0.1;     // 0.1° steps

    ElevationLUT lut;
    lut.build(radar, dem, cfg);

    // Test 1: target over sea — ground elevation must be 0 m
    std::cout << "\n--- Test 1: sea target 1000 m West, expect Ground Elev = 0 m ---\n";
    RadarMeasurement meas1 { 1000.0, 270.0, 0.0 };
    printResult(radar, meas1, computeTargetSeaLevel(radar, meas1, lut));

    // Test 2: airborne target above sea
    std::cout << "\n--- Test 2: airborne 1000 m West, elev +2°, expect AGL ~ 35 m ---\n";
    RadarMeasurement meas2 { 1000.0, 270.0, 2.0 };
    printResult(radar, meas2, computeTargetSeaLevel(radar, meas2, lut));

    return 0;
}
