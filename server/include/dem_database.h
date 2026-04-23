#pragma once
#include "dted_tile.h"
#include "types.h"
#include "constants.h"
#include <cstdint>
#include <string>

// Manages a collection of DEM tiles; routes lat/lon queries to the correct tile.
class DemDatabase {
public:
    enum class Format { DTED2, SRTM };

    // Load one tile from disk. origin_lat/lon = SW integer corner of the 1°×1° cell.
    void loadTile(const std::string& filepath, int origin_lat, int origin_lon, Format fmt);

    // Load all tiles within max_range_m of the radar position.
    // Skips files that don't exist in tiles_dir. Returns number of tiles loaded.
    int loadTilesAround(const LLA& radar, double max_range_m,
                        const std::string& tiles_dir, Format fmt);

    // Bicubic-interpolated ground elevation at an arbitrary lat/lon.
    // Throws if no tile covers the location.
    double getElevation(double lat_deg, double lon_deg) const;

    bool hasTile(double lat_deg, double lon_deg) const;

    // "e034n31.dt2"
    static std::string dted2Filename(int origin_lat, int origin_lon);

    // "N31E034.hgt"
    static std::string srtmFilename(int origin_lat, int origin_lon);

private:
    // Tiles are stored in a pre-allocated pool to avoid heap allocations.
    // Since objects are large (~26MB each), we use a static pointer to a pool 
    // or manage them carefully.
    static DtedTile tile_pool_[MAX_DEM_TILES];
    int num_tiles_ = 0;

    int findTileIndex(int lat, int lon) const;
};
