"""
Main entry point. Wires tile server, api client, controls, and map together.

Run:
    python gui/app.py [--server http://localhost:8080] [--tiles map_tiles/israel.mbtiles]
"""

import argparse
import os
import sqlite3
import sys
import time
from dash import Dash, Input, Output, State, callback, dcc, html, no_update
import dash_leaflet as dl

import tile_server as ts
import api_client as ac
import controls
import map_view

_GUI_DIR = os.path.dirname(os.path.abspath(__file__))

DEFAULT_SERVER = "http://127.0.0.1:8080"
DEFAULT_TILES  = os.path.join(_GUI_DIR, "map_tiles", "israel.mbtiles")


def parse_args():
    p = argparse.ArgumentParser()
    p.add_argument("--server", default=DEFAULT_SERVER)
    p.add_argument("--tiles",  default=DEFAULT_TILES)
    p.add_argument("--host",   default="127.0.0.1",
                   help="IP address remote browsers will use to reach this machine")
    return p.parse_args()


def main():
    args = parse_args()

    # ── Tile server ───────────────────────────────────────────────────────────
    tile_url = ts.start(args.tiles, host=args.host)
    time.sleep(0.5)

    with sqlite3.connect(args.tiles) as _con:
        _row = _con.execute("SELECT MAX(zoom_level) FROM tiles").fetchone()
    max_native_zoom = int(_row[0]) if _row and _row[0] is not None else 16

    # ── API client + startup health check ────────────────────────────────────
    client = ac.RadarApiClient(args.server)
    server_ok = client.health()
    print(f"[gui] Radar server ({args.server}): {'OK' if server_ok else 'UNREACHABLE'}")

    print("[gui] Checking Open Elevation API connectivity...")
    online_mode = ac.ping_open_elevation()
    print(f"[gui] Open Elevation API: {'ONLINE' if online_mode else 'OFFLINE (checkbox disabled)'}")

    # ── Dash app ──────────────────────────────────────────────────────────────
    app = Dash(__name__, suppress_callback_exceptions=True)

    if not server_ok:
        app.layout = html.Div([
            html.H2("⚠ Radar server not reachable", style={"color": "red"}),
            html.P(f"Could not connect to {args.server}"),
            html.P("Start the C++ server first:"),
            html.Pre("build\\Release\\radar_server.exe",
                     style={"backgroundColor": "#eee", "padding": "12px"}),
        ], style={"padding": "40px", "fontFamily": "sans-serif"})
        app.run(debug=False)
        return

    try:
        initial_radar = client.radar_info()
    except RuntimeError:
        initial_radar = None  # server running but radar not set yet

    app.layout = html.Div([
        dcc.Store(id="radar-store",   data=initial_radar),
        dcc.Store(id="targets-store", data=[]),
        dcc.Store(id="target-counter", data=0),
        controls.layout(online_mode),
        map_view.layout(initial_radar, tile_url, max_native_zoom),
    ], style={"display": "flex", "height": "100vh", "overflow": "hidden"})

    # ── Set radar callback ────────────────────────────────────────────────────
    @callback(
        Output("radar-store",      "data"),
        Output("radar-status-msg", "children"),
        Output("radar-status-msg", "style"),
        Output("targets-store",    "data",  allow_duplicate=True),
        Output("target-counter",   "data",  allow_duplicate=True),
        Input("btn-set-radar",     "n_clicks"),
        State("input-lat",         "value"),
        State("input-lon",         "value"),
        State("input-alt-msl",     "value"),
        prevent_initial_call=True,
    )
    def set_radar(n_clicks, lat, lon, alt):
        if not lat or not lon or alt is None:
            return no_update, "Please fill in all fields.", {"color": "red", "fontSize": "12px"}, no_update, no_update
        try:
            lat_f = float(lat)
            lon_f = float(lon)
        except (ValueError, TypeError):
            return no_update, "Latitude and longitude must be valid numbers.", {"color": "red", "fontSize": "12px"}, no_update, no_update
        try:
            radar = client.set_radar(lat_f, lon_f, float(alt))
            msg   = f"Radar set: {lat_f:.6f}°, {lon_f:.6f}°, {float(alt):.1f} m MSL"
            style = {"color": "green", "fontSize": "12px"}
            return radar, msg, style, [], 0
        except RuntimeError as e:
            return no_update, str(e), {"color": "red", "fontSize": "12px"}, no_update, no_update

    # ── Update radar layer when radar-store changes ───────────────────────────
    @callback(
        Output("radar-layer", "children"),
        Input("radar-store",  "data"),
    )
    def update_radar_layer(radar):
        return map_view.build_radar_layer(radar or {})

    # ── Target query callbacks ────────────────────────────────────────────────
    @callback(
        Output("targets-store",  "data"),
        Output("target-counter", "data"),
        Output("error-msg",      "children"),
        Input("btn-add",         "n_clicks"),
        Input("btn-clear",       "n_clicks"),
        State("input-range",     "value"),
        State("input-azimuth",   "value"),
        State("input-elevation", "value"),
        State("chk-open-elevation", "value"),
        State("targets-store",   "data"),
        State("target-counter",  "data"),
        prevent_initial_call=True,
    )
    def handle_buttons(add_clicks, clear_clicks, range_m, azimuth, elevation,
                       use_open_elev, targets, counter):
        from dash import ctx
        if ctx.triggered_id == "btn-clear":
            return [], 0, ""

        try:
            result = client.query(range_m, azimuth, elevation)
        except RuntimeError as e:
            return no_update, no_update, str(e)

        if online_mode and use_open_elev:
            lat_t, lon_t = result["lat_deg"], result["lon_deg"]
            try:
                result["open_elevation_m"] = ac.get_open_elevation(lat_t, lon_t)
            except Exception as e:
                print(f"[gui] Open-Elevation failed: {e}")
                result["open_elevation_m"] = None
            try:
                result["open_meteo_elevation_m"] = ac.get_open_meteo_elevation(lat_t, lon_t)
            except Exception as e:
                print(f"[gui] Open-Meteo failed: {e}")
                result["open_meteo_elevation_m"] = None

        counter += 1
        result["id"]            = counter
        result["azimuth_deg"]   = azimuth
        result["elevation_deg"] = elevation
        targets.append(result)
        return targets, counter, ""

    @callback(
        Output("target-layer", "children"),
        Input("targets-store", "data"),
    )
    def update_map(targets):
        return map_view.build_target_markers(targets)

    @callback(
        Output("conv-ll-to-utm-result", "children"),
        Input("btn-ll-to-utm", "n_clicks"),
        State("conv-lat", "value"),
        State("conv-lon", "value"),
        prevent_initial_call=True,
    )
    def convert_ll_to_utm(n_clicks, lat, lon):
        if not lat or not lon:
            return "Enter lat and lon."
        try:
            lat_f = float(lat)
            lon_f = float(lon)
        except (ValueError, TypeError):
            return "Lat and lon must be numbers."
        try:
            r = client.convert_ll_to_utm(lat_f, lon_f)
            return (f"E {r['easting']:.1f}  N {r['northing']:.1f}  "
                    f"Zone {r['zone']}{r['hemisphere']}")
        except RuntimeError as e:
            return str(e)

    @callback(
        Output("conv-utm-to-ll-result", "children"),
        Input("btn-utm-to-ll", "n_clicks"),
        State("conv-easting",   "value"),
        State("conv-northing",  "value"),
        State("conv-zone",      "value"),
        State("conv-hemisphere","value"),
        prevent_initial_call=True,
    )
    def convert_utm_to_ll(n_clicks, easting, northing, zone, hemisphere):
        if easting is None or northing is None or zone is None:
            return "Enter easting, northing, and zone."
        try:
            r = client.convert_utm_to_ll(float(easting), float(northing),
                                         int(zone), hemisphere)
            return f"Lat {r['lat_deg']:.7f}°  Lon {r['lon_deg']:.7f}°"
        except RuntimeError as e:
            return str(e)

    @callback(
        Output("elevation-bar", "children"),
        Input("map",            "clickData"),
        prevent_initial_call=True,
    )
    def show_elevation(click_data):
        if not click_data:
            return "Click on the map to see elevation"
        latlng = click_data.get("latlng", {})
        lat = latlng.get("lat")
        lon = latlng.get("lng")
        if lat is None or lon is None:
            return "Click on the map to see elevation"
        try:
            elev = client.get_elevation(lat, lon)
            return f"Lat: {lat:.5f}°  |  Lon: {lon:.5f}°  |  Elev: {elev:.1f} m MSL"
        except RuntimeError:
            return f"Lat: {lat:.5f}°  |  Lon: {lon:.5f}°  |  Elev: N/A"

    app.run(debug=False, host="0.0.0.0", port=8050)


if __name__ == "__main__":
    main()
