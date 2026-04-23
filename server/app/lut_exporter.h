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
// Tiles are loaded per request and released on destruction.
class LutExporter {
public:
    LutExporter(const LLA& radar, double max_range_m, const std::string& tiles_dir);

    LutExportData exportData() const;
    void          saveToFile(const LutExportData& data, const std::string& path) const;

private:
    LLA         radar_;
    double      max_range_m_;
    double      range_step_m_ = 15.0;
    double      az_step_deg_  = 0.1;
    DemDatabase dem_;

    static int32_t results_buffer_[MAX_LUT_RANGES * MAX_LUT_AZIMUTHS];
};
