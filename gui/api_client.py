"""
HTTP client for the C++ radar server.
Only this file knows the server URL.
"""

import requests

class RadarApiClient:
    def __init__(self, base_url: str = "http://localhost:8080"):
        self._base = base_url.rstrip("/")

    def health(self) -> bool:
        try:
            r = requests.get(f"{self._base}/health", timeout=2)
            return r.status_code == 200
        except requests.RequestException:
            return False

    def query(self, range_m: float, azimuth_deg: float, elevation_deg: float) -> dict:
        """
        Returns dict with keys: lat_deg, lon_deg, alt_msl_m, ground_elev_m, agl_m, horiz_range_m
        Raises RuntimeError with a user-facing message on 4xx/5xx or connection failure.
        """
        payload = {
            "range_m":       range_m,
            "azimuth_deg":   azimuth_deg,
            "elevation_deg": elevation_deg,
        }
        try:
            r = requests.post(f"{self._base}/query", json=payload, timeout=5)
        except requests.RequestException as e:
            raise RuntimeError(f"Cannot reach radar server at {self._base}: {e}") from e

        if r.status_code == 200:
            return r.json()

        # Server returned a structured error
        try:
            msg = r.json().get("error", r.text)
        except Exception:
            msg = r.text
        raise RuntimeError(f"Server error {r.status_code}: {msg}")
