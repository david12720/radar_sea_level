"""
HTTP client for the C++ radar server and optional elevation APIs.
Only this file knows the server URLs.
"""

import struct
import requests
import urllib3

# Suppress InsecureRequestWarning for general use
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

OPEN_ELEVATION_URL = "https://api.open-elevation.com/api/v1/lookup"
OPEN_METEO_URL     = "https://api.open-meteo.com/v1/elevation"
OPEN_TOPO_URL      = "https://api.opentopodata.org/v1/srtm30m"

DEFAULT_HEADERS = {
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36"
}

def ping_elevation_api(timeout: float = 3.0) -> bool:
    """Quick check for online mode. Skips SSL verification."""
    try:
        r = requests.get(OPEN_METEO_URL, params={"latitude":0,"longitude":0}, 
                         timeout=timeout, verify=False)
        return r.status_code == 200
    except Exception:
        return False

def ping_open_elevation(timeout: float = 3.0) -> bool:
    """Legacy alias for backward compatibility."""
    return ping_elevation_api(timeout)

def get_open_elevation(lat: float, lon: float, timeout: float = 3.0) -> float:
    """Returns terrain elevation MSL (m) from Open-Elevation. Skips SSL verification."""
    try:
        payload = {"locations": [{"latitude": lat, "longitude": lon}]}
        r = requests.post(OPEN_ELEVATION_URL, json=payload, timeout=timeout, verify=False)
        if r.status_code == 200:
            return float(r.json()["results"][0]["elevation"])
    except Exception:
        pass
    return None

def get_open_meteo_elevation(lat: float, lon: float, timeout: float = 3.0) -> float:
    """Returns terrain elevation MSL (m) from Open-Meteo. Skips SSL verification."""
    try:
        r = requests.get(OPEN_METEO_URL, params={"latitude": lat, "longitude": lon},
                         timeout=timeout, verify=False, headers=DEFAULT_HEADERS)
        if r.status_code == 200:
            return float(r.json()["elevation"][0])
    except Exception:
        pass
    return None

def get_open_topo_elevation(lat: float, lon: float, timeout: float = 3.0) -> float:
    """Returns terrain elevation MSL (m) from Open-Topo. Skips SSL verification."""
    try:
        r = requests.get(OPEN_TOPO_URL, params={"locations": f"{lat},{lon}"},
                         timeout=timeout, verify=False, headers=DEFAULT_HEADERS)
        if r.status_code == 200:
            return float(r.json()["results"][0]["elevation"])
    except Exception:
        pass
    return None

class RadarApiClient:
    def __init__(self, base_url: str = "http://127.0.0.1:8080"):
        self._base = base_url.rstrip("/")

    def health(self) -> bool:
        try:
            r = requests.get(f"{self._base}/health", timeout=2, verify=False)
            return r.status_code == 200
        except requests.RequestException:
            return False

    def radar_info(self) -> dict:
        try:
            r = requests.get(f"{self._base}/radar", timeout=2, verify=False)
            if r.status_code == 200: return r.json()
        except Exception: pass
        return None

    def set_radar(self, lat_deg: float, lon_deg: float, agl_m: float) -> dict:
        payload = {"lat_deg": lat_deg, "lon_deg": lon_deg, "agl_m": agl_m}
        r = requests.post(f"{self._base}/radar", json=payload, timeout=120, verify=False)
        if r.status_code == 200: return r.json()
        raise RuntimeError(r.text)

    def get_lut(self) -> tuple:
        r = requests.get(f"{self._base}/lut", timeout=60, verify=False)
        if r.status_code != 200: raise RuntimeError(r.text)
        data = r.content
        az_count, range_count = struct.unpack_from("<II", data, 0)
        return data[8:], {"az_count": int(az_count), "range_count": int(range_count), "az_step_deg": 0.1, "range_step_m": 15.0}

    def get_elevation(self, lat_deg: float, lon_deg: float) -> float:
        r = requests.get(f"{self._base}/elevation", params={"lat": lat_deg, "lon": lon_deg}, timeout=3, verify=False)
        if r.status_code == 200: return r.json()["elev_m"]
        return 0.0

    def convert_ll_to_utm(self, lat_deg: float, lon_deg: float) -> dict:
        r = requests.post(f"{self._base}/convert", json={"direction": "ll_to_utm", "lat_deg": lat_deg, "lon_deg": lon_deg}, timeout=5, verify=False)
        if r.status_code == 200: return r.json()
        raise RuntimeError(r.text)

    def convert_utm_to_ll(self, easting: float, northing: float, zone: int, hemisphere: str) -> dict:
        r = requests.post(f"{self._base}/convert", json={"direction": "utm_to_ll", "easting": easting, "northing": northing, "zone": zone, "hemisphere": hemisphere}, timeout=5, verify=False)
        if r.status_code == 200: return r.json()
        raise RuntimeError(r.text)

    def query(self, range_m: float, azimuth_deg: float, elevation_deg: float, terrain_msl_m: float, earth_model: str = None) -> dict:
        payload = {"range_m": range_m, "azimuth_deg": azimuth_deg, "elevation_deg": elevation_deg, "terrain_msl_m": terrain_msl_m}
        if earth_model: payload["earth_model"] = earth_model
        r = requests.post(f"{self._base}/query", json=payload, timeout=5, verify=False)
        if r.status_code == 200: return r.json()
        raise RuntimeError(r.text)
