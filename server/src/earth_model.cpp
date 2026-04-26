#include "earth_model.h"
#include "constants.h"
#include <cmath>
#include <stdexcept>

namespace {

// Flat-earth ground projection: linear offsets scaled by EARTH_RADIUS.
// Longitude step shrinks toward the poles by 1/cos(lat).
GroundPoint ground_point_flat(const LLA& radar, double horiz_range_m, double azimuth_deg)
{
    const double az_r  = azimuth_deg * DEG2RAD;
    const double lat_r = radar.lat_deg * DEG2RAD;
    return {
        radar.lat_deg + (horiz_range_m * std::cos(az_r) / EARTH_RADIUS) * RAD2DEG,
        radar.lon_deg + (horiz_range_m * std::sin(az_r) / (EARTH_RADIUS * std::cos(lat_r))) * RAD2DEG
    };
}

// Haversine destination: start lat/lon + initial bearing + ground-arc distance → end lat/lon.
// Formula: www.movable-type.co.uk/scripts/latlong.html
GroundPoint ground_point_sphere(const LLA& radar, double arc_m,
                                double azimuth_deg, double r_eff_m)
{
    const double phi1    = radar.lat_deg * DEG2RAD;
    const double lam1    = radar.lon_deg * DEG2RAD;
    const double brng    = azimuth_deg * DEG2RAD;
    const double delta   = arc_m / r_eff_m;
    const double sin_p1  = std::sin(phi1);
    const double cos_p1  = std::cos(phi1);
    const double sin_d   = std::sin(delta);
    const double cos_d   = std::cos(delta);
    const double phi2    = std::asin(sin_p1*cos_d + cos_p1*sin_d*std::cos(brng));
    const double lam2    = lam1 + std::atan2(std::sin(brng)*sin_d*cos_p1,
                                             cos_d - sin_p1*std::sin(phi2));
    const double lon_deg = std::fmod(lam2 * RAD2DEG + 540.0, 360.0) - 180.0;
    return { phi2 * RAD2DEG, lon_deg };
}

class FlatEarthModel : public IEarthModel {
public:
    TargetResult propagate(const LLA& radar, const RadarMeasurement& meas) const override
    {
        const double el_r  = meas.elevation_deg * DEG2RAD;
        const double horiz = meas.range_m * std::cos(el_r);
        const double vert  = meas.range_m * std::sin(el_r);
        const GroundPoint gp = ground_point_flat(radar, horiz, meas.azimuth_deg);
        TargetResult res;
        res.horizontal_range_m = horiz;
        res.vertical_offset_m  = vert;
        res.position = { gp.lat_deg, gp.lon_deg, radar.alt_m + vert };
        return res;
    }
    const char* name() const override { return "flat"; }
};

// 3D spherical geometry. Radar sits at height radar.alt_m above a sphere of radius r_eff_.
// Beam is a straight line in the radar's local tangent plane at elevation angle `el`.
// Target altitude  : |C→T| − r_eff_
// Ground-arc range : r_eff_ · atan2(R·cos(el),  r_eff_+radar.alt + R·sin(el))
// r_eff_ = EARTH_RADIUS             → true sphere
// r_eff_ = (4/3) · EARTH_RADIUS     → standard atmospheric-refraction model (k4/3)
class SphericalEarthModel : public IEarthModel {
public:
    SphericalEarthModel(double r_eff_m, const char* model_name)
        : r_eff_(r_eff_m), name_(model_name) {}

    TargetResult propagate(const LLA& radar, const RadarMeasurement& meas) const override
    {
        const double el_r   = meas.elevation_deg * DEG2RAD;
        const double r_s    = meas.range_m;
        const double rc     = r_eff_ + radar.alt_m;
        const double v_comp = r_s * std::sin(el_r);
        const double h_comp = r_s * std::cos(el_r);

        const double rt         = std::sqrt(r_s*r_s + rc*rc + 2.0*rc*v_comp);
        const double target_alt = rt - r_eff_;
        const double theta      = std::atan2(h_comp, rc + v_comp);
        const double ground_arc = r_eff_ * theta;

        // Ground arc distance applies to the REAL Earth surface (R = 6371 km).
        // For k43, r_eff_ = (4/3)·R is a fictional radius used only for altitude —
        // target lat/lon must be placed on the real Earth.
        const GroundPoint gp = ground_point_sphere(radar, ground_arc, meas.azimuth_deg, EARTH_RADIUS);

        TargetResult res;
        res.horizontal_range_m = ground_arc;
        res.vertical_offset_m  = target_alt - radar.alt_m;
        res.position = { gp.lat_deg, gp.lon_deg, target_alt };
        return res;
    }
    const char* name() const override { return name_; }

private:
    double      r_eff_;
    const char* name_;
};

} // namespace

const IEarthModel& getEarthModel(const std::string& name)
{
    static const FlatEarthModel flat;
    static const SphericalEarthModel sphere(EARTH_RADIUS, "sphere");
    static const SphericalEarthModel k43((4.0/3.0) * EARTH_RADIUS, "k43");

    if (name.empty() || name == "flat") return flat;
    if (name == "sphere") return sphere;
    if (name == "k43") return k43;
    throw std::invalid_argument("unknown earth_model '" + name + "' (expected flat|sphere|k43)");
}
