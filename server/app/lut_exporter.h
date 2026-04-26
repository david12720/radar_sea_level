#pragma once
#include "types.h"
#include "dem_database.h"
#include <cstdint>
#include <string>
#include <vector>

struct LutExportData {
    int32_t*             cells;  // points to a static buffer: [az_idx * range_count + range_idx]
    uint32_t             az_count;
    uint32_t             range_count;
    size_t               total_cells;
};

// Loads DEM tiles relevant to the given radar position and exports ground elevation
// as a flat int32 array in [azimuth][range] order (az outer, range inner).
/**
 * High-speed terrain grid generator.
 *
 * This class builds a polar Look-Up Table (LUT) of terrain elevations relative 
 * to the radar position. It samples the DEM at 0.1° azimuth and 15m range intervals.
 * Uses a static pre-allocated buffer for the result to avoid heap fragmentation.
 */
class LutExporter {
public:
    enum class AltMode { AGL, MSL };

    /**
     * Initializes the exporter and pre-loads required DEM tiles.
     * @param radar       Source position for the polar scan.
     * @param max_range_m Maximum radial distance to scan.
     * @param tiles_dir   Path to the DEM (HGT) directory.
     * @param alt_mode    Whether radar alt is interpreted as Above Ground or Mean Sea Level.
     */
    LutExporter(const LLA& radar, double max_range_m, const std::string& tiles_dir,
                AltMode alt_mode = AltMode::AGL);

    /**
     * Performs the full polar scan of the terrain.
     * Iterates through ~14.4M points using bicubic interpolation for each.
     * @return Metadata and a pointer to the populated static results buffer.
     */
    LutExportData exportData() const;

    /** Binary persistence of the LUT to disk. */
    void          saveToFile(const LutExportData& data, const std::string& path) const;

    /** Returns the exact terrain height (MSL) found at the radar's ground point. */
    double radarTerrainM() const { return terrain_m_; }

    /** Returns the count of 1°x1° tiles actually loaded during the scan. */
    int    tilesLoaded()   const { return tiles_loaded_; }

private:
    LLA         radar_;
    double      max_range_m_;
    double      range_step_m_ = 15.0;
    double      az_step_deg_  = 0.1;
    double      terrain_m_    = 0.0;
    int         tiles_loaded_ = 0;
    DemDatabase dem_;

    static int32_t results_buffer_[MAX_LUT_RANGES * MAX_LUT_AZIMUTHS];
};
