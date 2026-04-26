#pragma once
#include "dted_tile.h"
#include "types.h"
#include "constants.h"
#include <cstdint>
#include <string>

/**
 * Manages a collection of Digital Elevation Model (DEM) tiles.
 *
 * This database acts as a memory-efficient cache that routes lat/lon queries 
 * to the correct 1°x1° tile. It uses a static pool of pre-allocated buffers 
 * to avoid heap allocations during tile loading.
 */
class DemDatabase {
public:
    enum class Format { DTED2, SRTM };

    /** 
     * Loads one tile from disk into the next available slot in the pool.
     * origin_lat/lon represent the South-West integer corner of the tile.
     */
    void loadTile(const std::string& filepath, int origin_lat, int origin_lon, Format fmt);

    /**
     * Bulk-loads all tiles within a radial distance of the radar.
     * Automatically calculates the required bounding box (accounting for 
     * longitude convergence at higher latitudes).
     * @return Number of tiles successfully loaded into the pool.
     */
    int loadTilesAround(const LLA& radar, double max_range_m,
                        const std::string& tiles_dir, Format fmt);

    /**
     * Returns the ground elevation MSL at an arbitrary coordinate.
     * Uses bicubic interpolation (4x4 sampling) for sub-post accuracy.
     * @throws std::runtime_error if no tile covering the point is loaded.
     */
    double getElevation(double lat_deg, double lon_deg) const;

    /** Returns true if a tile covering the coordinate is already in the pool. */
    bool hasTile(double lat_deg, double lon_deg) const;

    /** Resets the pool counter (does not zero the memory, just invalidates slots). */
    void clear() { num_tiles_ = 0; }

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
