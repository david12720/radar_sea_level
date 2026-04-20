"""
Tests for RadarApiClient.
Requires the C++ radar_server to be running on localhost:8080.
Run from project root: python -m pytest gui/tests/ -v
"""

import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

import pytest
from api_client import RadarApiClient

BASE_URL = "http://localhost:8080"


@pytest.fixture(scope="module")
def client():
    c = RadarApiClient(BASE_URL)
    if not c.health():
        pytest.skip("radar_server not running — start it before running these tests")
    return c


def test_health(client):
    assert client.health() is True


def test_sea_target_ground_elev_zero(client):
    r = client.query(range_m=1000, azimuth_deg=270, elevation_deg=0)
    assert abs(r["ground_elev_m"]) < 2.0, f"expected ~0, got {r['ground_elev_m']}"


def test_result_keys_present(client):
    r = client.query(range_m=500, azimuth_deg=0, elevation_deg=0)
    for key in ("lat_deg", "lon_deg", "alt_msl_m", "ground_elev_m", "agl_m", "horiz_range_m"):
        assert key in r, f"missing key: {key}"


def test_agl_equals_alt_minus_ground(client):
    r = client.query(range_m=2000, azimuth_deg=45, elevation_deg=2)
    assert abs(r["agl_m"] - (r["alt_msl_m"] - r["ground_elev_m"])) < 0.01


def test_validation_range_too_large(client):
    with pytest.raises(RuntimeError, match="400|validation"):
        client.query(range_m=99999, azimuth_deg=0, elevation_deg=0)


def test_validation_bad_azimuth(client):
    with pytest.raises(RuntimeError, match="400|validation"):
        client.query(range_m=1000, azimuth_deg=360, elevation_deg=0)


def test_validation_bad_elevation(client):
    with pytest.raises(RuntimeError, match="400|validation"):
        client.query(range_m=1000, azimuth_deg=0, elevation_deg=50)


def test_unreachable_server():
    bad = RadarApiClient("http://localhost:19999")
    assert bad.health() is False
    with pytest.raises(RuntimeError, match="Cannot reach"):
        bad.query(1000, 0, 0)
