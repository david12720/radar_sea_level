#include "coord_convert.h"
#include "constants.h"
#include <cmath>

// WGS84 ellipsoid
static constexpr double WGS84_A  = 6378137.0;
static constexpr double WGS84_F  = 1.0 / 298.257223563;
static constexpr double WGS84_E2 = 2*WGS84_F - WGS84_F*WGS84_F;
static constexpr double WGS84_EP2 = WGS84_E2 / (1.0 - WGS84_E2);
static constexpr double UTM_K0   = 0.9996;

UtmPoint ll_to_utm(double lat_deg, double lon_deg)
{
    int zone = static_cast<int>(std::floor((lon_deg + 180.0) / 6.0)) + 1;
    double lon0_r = ((zone - 1) * 6.0 - 180.0 + 3.0) * DEG2RAD;

    double lat_r = lat_deg * DEG2RAD;
    double lon_r = lon_deg * DEG2RAD;

    double sin_lat = std::sin(lat_r);
    double cos_lat = std::cos(lat_r);
    double tan_lat = std::tan(lat_r);

    double N = WGS84_A / std::sqrt(1.0 - WGS84_E2 * sin_lat * sin_lat);
    double T = tan_lat * tan_lat;
    double C = WGS84_EP2 * cos_lat * cos_lat;
    double A = cos_lat * (lon_r - lon0_r);

    double e4 = WGS84_E2 * WGS84_E2;
    double e6 = e4 * WGS84_E2;
    double M = WGS84_A * (
        (1.0 - WGS84_E2/4.0 - 3.0*e4/64.0 - 5.0*e6/256.0) * lat_r
      - (3.0*WGS84_E2/8.0 + 3.0*e4/32.0 + 45.0*e6/1024.0) * std::sin(2.0*lat_r)
      + (15.0*e4/256.0 + 45.0*e6/1024.0) * std::sin(4.0*lat_r)
      - (35.0*e6/3072.0) * std::sin(6.0*lat_r));

    double A2=A*A, A3=A2*A, A4=A3*A, A5=A4*A, A6=A5*A;
    double T2=T*T;

    double easting = UTM_K0 * N * (
        A + (1.0-T+C)*A3/6.0
        + (5.0-18.0*T+T2+72.0*C-58.0*WGS84_EP2)*A5/120.0)
        + 500000.0;

    double northing = UTM_K0 * (M + N*tan_lat*(
        A2/2.0
        + (5.0-T+9.0*C+4.0*C*C)*A4/24.0
        + (61.0-58.0*T+T2+600.0*C-330.0*WGS84_EP2)*A6/720.0));

    char hemisphere = 'N';
    if (lat_deg < 0.0) { northing += 10000000.0; hemisphere = 'S'; }

    return { easting, northing, zone, hemisphere };
}

void utm_to_ll(const UtmPoint& utm, double& lat_deg, double& lon_deg)
{
    double x = utm.easting - 500000.0;
    double y = utm.northing;
    if (utm.hemisphere == 'S' || utm.hemisphere == 's') y -= 10000000.0;

    double e4 = WGS84_E2 * WGS84_E2;
    double e6 = e4 * WGS84_E2;

    double M  = y / UTM_K0;
    double mu = M / (WGS84_A * (1.0 - WGS84_E2/4.0 - 3.0*e4/64.0 - 5.0*e6/256.0));

    double e1   = (1.0 - std::sqrt(1.0 - WGS84_E2)) / (1.0 + std::sqrt(1.0 - WGS84_E2));
    double e1_2 = e1*e1, e1_3=e1_2*e1, e1_4=e1_3*e1;

    double phi1 = mu
        + (3.0*e1/2.0     - 27.0*e1_3/32.0) * std::sin(2.0*mu)
        + (21.0*e1_2/16.0 - 55.0*e1_4/32.0) * std::sin(4.0*mu)
        + (151.0*e1_3/96.0)                  * std::sin(6.0*mu)
        + (1097.0*e1_4/512.0)                * std::sin(8.0*mu);

    double sin_phi1 = std::sin(phi1);
    double cos_phi1 = std::cos(phi1);
    double tan_phi1 = std::tan(phi1);

    double N1 = WGS84_A / std::sqrt(1.0 - WGS84_E2*sin_phi1*sin_phi1);
    double T1 = tan_phi1 * tan_phi1;
    double C1 = WGS84_EP2 * cos_phi1 * cos_phi1;
    double R1 = WGS84_A * (1.0-WGS84_E2) / std::pow(1.0 - WGS84_E2*sin_phi1*sin_phi1, 1.5);
    double D  = x / (N1 * UTM_K0);

    double D2=D*D, D3=D2*D, D4=D3*D, D5=D4*D, D6=D5*D;
    double T1_2=T1*T1;

    double lat_r = phi1 - (N1*tan_phi1/R1) * (
        D2/2.0
        - (5.0+3.0*T1+10.0*C1-4.0*C1*C1-9.0*WGS84_EP2)*D4/24.0
        + (61.0+90.0*T1+298.0*C1+45.0*T1_2-252.0*WGS84_EP2-3.0*C1*C1)*D6/720.0);

    double lon0_r = ((utm.zone - 1) * 6.0 - 180.0 + 3.0) * DEG2RAD;
    double lon_r  = lon0_r + (
        D - (1.0+2.0*T1+C1)*D3/6.0
        + (5.0-2.0*C1+28.0*T1-3.0*C1*C1+8.0*WGS84_EP2+24.0*T1_2)*D5/120.0)
        / cos_phi1;

    lat_deg = lat_r * RAD2DEG;
    lon_deg = lon_r * RAD2DEG;
}
