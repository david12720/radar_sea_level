"""
HTTP client for the C++ radar server and optional elevation APIs.
Only this file knows the server URLs.
"""

import requests
import urllib3

# Suppress InsecureRequestWarning when using verify=False
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

OPEN_ELEVATION_URL = "https://api.open-elevation.com/api/v1/lookup"
OPEN_METEO_URL     = "https://api.open-meteo.com/v1/elevation"


def ping_open_elevation(timeout: float = 3.0) -> bool:
    """Returns True if the Open Elevation API is reachable. Skips SSL verification."""
    try:
        # verify=False skips SSL certificate validation (common issue with local proxies/certs)
        r = requests.post(OPEN_ELEVATION_URL,
                          json={"locations": [{"latitude": 0, "longitude": 0}]},
                          timeout=timeout,
                          verify=False)
        if r.status_code == 200:
            return True
        print(f"[gui] Open Elevation ping failed with status {r.status_code}: {r.text[:100]}")
        return False
    except requests.exceptions.SSLError as e:
        print(f"[gui] Open Elevation SSL Error (even with verify=False): {e}")
        return False
    except requests.exceptions.Timeout:
        print("[gui] Open Elevation ping failed: Connection timed out")
        return False
    except requests.exceptions.ConnectionError:
        print("[gui] Open Elevation ping failed: Could not connect to host (check internet)")
        return False
    except requests.RequestException as e:
        print(f"[gui] Open Elevation ping failed: {e}")
        return False


def get_open_elevation(lat: float, lon: float, timeout: float = 5.0) -> float:
    """Returns terrain elevation MSL (m) from Open Elevation API (SRTM). Skips SSL verification."""
    try:
        r = requests.post(OPEN_ELEVATION_URL,
                          json={"locations": [{"latitude": lat, "longitude": lon}]},
                          timeout=timeout,
                          verify=False)
    except requests.RequestException as e:
        raise RuntimeError(f"Open Elevation unreachable: {e}") from e
    if r.status_code != 200:
        raise RuntimeError(f"Open Elevation error {r.status_code}: {r.text}")
    try:
        return float(r.json()["results"][0]["elevation"])
    except (KeyError, IndexError, ValueError) as e:
        raise RuntimeError(f"Unexpected Open Elevation response: {e}") from e


def get_open_meteo_elevation(lat: float, lon: float, timeout: float = 5.0) -> float:
    """Returns terrain elevation MSL (m) from Open-Meteo API (Copernicus DEM 90m). Skips SSL verification."""
    try:
        r = requests.get(OPEN_METEO_URL,
                         params={"latitude": lat, "longitude": lon},
                         timeout=timeout,
                         verify=False)
    except requests.RequestException as e:
        raise RuntimeError(f"Open-Meteo unreachable: {e}") from e
    if r.status_code != 200:
        raise RuntimeError(f"Open-Meteo error {r.status_code}: {r.text}")
    try:
        return float(r.json()["elevation"][0])
    except (KeyError, IndexError, ValueError) as e:
        raise RuntimeError(f"Unexpected Open-Meteo response: {e}") from e

class RadarApiClient:
    def __init__(self, base_url: str = "http://127.0.0.1:8080"):
        self._base = base_url.rstrip("/")

    def health(self) -> bool:
        try:
            r = requests.get(f"{self._base}/health", timeout=2)
            return r.status_code == 200
        except requests.RequestException:
            return False

    def radar_info(self) -> dict:
        """
        Returns dict with keys: lat_deg, lon_deg, alt_m, ground_elev_m, agl_m, max_range_m
        Raises RuntimeError on failure.
        """
        try:
            r = requests.get(f"{self._base}/radar", timeout=2)
        except requests.RequestException as e:
            raise RuntimeError(f"Cannot reach radar server at {self._base}: {e}") from e
        if r.status_code == 200:
            return r.json()
        raise RuntimeError(f"Server error {r.status_code}: {r.text}")

    def set_radar(self, lat_deg: float, lon_deg: float, alt_msl_m: float) -> dict:
        """
        Sets radar position (MSL). Returns same dict as radar_info().
        Raises RuntimeError on failure.
        """
        payload = {"lat_deg": lat_deg, "lon_deg": lon_deg, "alt_msl_m": alt_msl_m}
        try:
            r = requests.post(f"{self._base}/radar", json=payload, timeout=10)
        except requests.RequestException as e:
            raise RuntimeError(f"Cannot reach radar server at {self._base}: {e}") from e
        if r.status_code == 200:
            return r.json()
        try:
            msg = r.json().get("error", r.text)
        except Exception:
            msg = r.text
        raise RuntimeError(f"Server error {r.status_code}: {msg}")

    def get_elevation(self, lat_deg: float, lon_deg: float) -> float:
        """Returns terrain elevation MSL in meters. Raises RuntimeError on failure."""
        try:
            r = requests.get(f"{self._base}/elevation",
                             params={"lat": lat_deg, "lon": lon_deg}, timeout=3)
        except requests.RequestException as e:
            raise RuntimeError(f"Cannot reach radar server: {e}") from e
        if r.status_code == 200:
            return r.json()["elev_m"]
        try:
            msg = r.json().get("error", r.text)
        except Exception:
            msg = r.text
        raise RuntimeError(f"Server error {r.status_code}: {msg}")

    def convert_ll_to_utm(self, lat_deg: float, lon_deg: float) -> dict:
        """
        Returns dict with keys: easting, northing, zone, hemisphere ('N'/'S').
        Raises RuntimeError on failure.
        """
        payload = {"direction": "ll_to_utm", "lat_deg": lat_deg, "lon_deg": lon_deg}
        try:
            r = requests.post(f"{self._base}/convert", json=payload, timeout=5)
        except requests.RequestException as e:
            raise RuntimeError(f"Cannot reach radar server: {e}") from e
        if r.status_code == 200:
            return r.json()
        try:
            msg = r.json().get("error", r.text)
        except Exception:
            msg = r.text
        raise RuntimeError(f"Server error {r.status_code}: {msg}")

    def convert_utm_to_ll(self, easting: float, northing: float,
                          zone: int, hemisphere: str) -> dict:
        """
        Returns dict with keys: lat_deg, lon_deg.
        hemisphere: 'N' or 'S'
        Raises RuntimeError on failure.
        """
        payload = {"direction": "utm_to_ll", "easting": easting, "northing": northing,
                   "zone": zone, "hemisphere": hemisphere.upper()}
        try:
            r = requests.post(f"{self._base}/convert", json=payload, timeout=5)
        except requests.RequestException as e:
            raise RuntimeError(f"Cannot reach radar server: {e}") from e
        if r.status_code == 200:
            return r.json()
        try:
            msg = r.json().get("error", r.text)
        except Exception:
            msg = r.text
        raise RuntimeError(f"Server error {r.status_code}: {msg}")

    def query(self, range_m: float, azimuth_deg: float, elevation_deg: float,
              ground_elevation_m: float = None, earth_model: str = None) -> dict:
        """
        Returns dict with keys: lat_deg, lon_deg, alt_msl_m, ground_elev_m, agl_m,
                                horiz_range_m, relative_elev_deg, earth_model
        If ground_elevation_m is provided, the server uses it directly (no DEM lookup).
        earth_model: 'flat' (default) | 'sphere' | 'k43'
        Raises RuntimeError with a user-facing message on 4xx/5xx or connection failure.
        """
        payload = {
            "range_m":       range_m,
            "azimuth_deg":   azimuth_deg,
            "elevation_deg": elevation_deg,
        }
        if ground_elevation_m is not None:
            payload["ground_elevation_m"] = ground_elevation_m
        if earth_model:
            payload["earth_model"] = earth_model
        try:
            r = requests.post(f"{self._base}/query", json=payload, timeout=5)
        except requests.RequestException as e:
            raise RuntimeError(f"Cannot reach radar server at {self._base}: {e}") from e

        if r.status_code == 200:
            return r.json()

        try:
            msg = r.json().get("error", r.text)
        except Exception:
            msg = r.text
        raise RuntimeError(f"Server error {r.status_code}: {msg}")
