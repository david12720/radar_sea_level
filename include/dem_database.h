#pragma once
#include "dted_tile.h"
#include <string>
#include <unordered_map>

// Manages a collection of DEM tiles; routes lat/lon queries to the correct tile.
class DemDatabase {
public:
    enum class Format { DTED2, SRTM };

    // Load one tile from disk. origin_lat/lon = SW integer corner of the 1°×1° cell.
    void loadTile(const std::string& filepath, int origin_lat, int origin_lon, Format fmt);

    // Bicubic-interpolated ground elevation at an arbitrary lat/lon.
    // Throws if no tile covers the location.
    double getElevation(double lat_deg, double lon_deg) const;

    bool hasTile(double lat_deg, double lon_deg) const;

    // "e034n31.dt2"
    static std::string dted2Filename(int origin_lat, int origin_lon);

    // "N31E034.hgt"
    static std::string srtmFilename(int origin_lat, int origin_lon);

private:
    std::unordered_map<std::string, DtedTile> tiles_;

    static std::string tileKey(int lat, int lon);
};
