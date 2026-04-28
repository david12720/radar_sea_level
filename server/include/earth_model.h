#pragma once
#include "types.h"
#include <memory>
#include <string>

// Strategy for propagating a radar slant-range measurement to a target position.
// Concrete implementations: FlatEarth (tangent-plane), Spherical (R = 6371 km),
// Refractive (4/3 Earth radius — standard atmospheric refraction model).
class IEarthModel {
public:
    virtual ~IEarthModel() = default;

    // Given the radar LLA and a (range, azimuth, elevation) measurement, computes
    // the target's position (lat/lon/alt MSL), horizontal_range_m and vertical_offset_m.
    // Does NOT set terrain_msl_m, target_height_agl_m, or relative_elevation_deg —
    // those are model-independent and are filled in by the caller.
    virtual TargetResult propagate(const LLA& radar,
                                   const RadarMeasurement& meas) const = 0;

    virtual const char* name() const = 0;
};

// Accepted: "" or "flat", "sphere", "k43".
// Throws std::invalid_argument on unknown names.
const IEarthModel& getEarthModel(const std::string& name);
