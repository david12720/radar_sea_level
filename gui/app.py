"""
Main entry point. Wires tile server, api client, controls, and map together.

Run:
    python gui/app.py [--server http://localhost:8080] [--tiles map_tiles/israel.mbtiles]
                      [--lat 32.08] [--lon 34.76] [--max-range 50000]
"""

import argparse
import os
import sys
import time
from dash import Dash, Input, Output, State, callback, dcc, html, no_update
import dash_leaflet as dl

import tile_server as ts
import api_client as ac
import controls
import map_view

_PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

DEFAULT_SERVER    = "http://localhost:8080"
DEFAULT_TILES     = os.path.join(_PROJECT_ROOT, "map_tiles", "israel.mbtiles")
DEFAULT_LAT       = 32.08
DEFAULT_LON       = 34.76
DEFAULT_MAX_RANGE = 50000.0


def parse_args():
    p = argparse.ArgumentParser()
    p.add_argument("--server",    default=DEFAULT_SERVER)
    p.add_argument("--tiles",     default=DEFAULT_TILES)
    p.add_argument("--lat",       type=float, default=DEFAULT_LAT)
    p.add_argument("--lon",       type=float, default=DEFAULT_LON)
    p.add_argument("--max-range", type=float, default=DEFAULT_MAX_RANGE, dest="max_range")
    return p.parse_args()


def main():
    args = parse_args()

    # ── Tile server ───────────────────────────────────────────────────────────
    tile_url = ts.start(args.tiles)
    time.sleep(0.5)  # let Flask thread bind

    # ── API client + startup health check ────────────────────────────────────
    client = ac.RadarApiClient(args.server)
    server_ok = client.health()

    # ── Dash app ──────────────────────────────────────────────────────────────
    app = Dash(__name__, suppress_callback_exceptions=True)

    if not server_ok:
        app.layout = html.Div([
            html.H2("⚠ Radar server not reachable", style={"color": "red"}),
            html.P(f"Could not connect to {args.server}"),
            html.P("Start the C++ server first:"),
            html.Pre(f"./radar_server --lat {args.lat} --lon {args.lon}",
                     style={"backgroundColor": "#eee", "padding": "12px"}),
        ], style={"padding": "40px", "fontFamily": "sans-serif"})
        app.run(debug=False)
        return

    app.layout = html.Div([
        dcc.Store(id="targets-store", data=[]),
        dcc.Store(id="target-counter", data=0),
        controls.layout(),
        map_view.layout(args.lat, args.lon, args.max_range, tile_url),
    ], style={"display": "flex", "height": "100vh", "overflow": "hidden"})

    @callback(
        Output("targets-store",  "data"),
        Output("target-counter", "data"),
        Output("error-msg",      "children"),
        Input("btn-add",         "n_clicks"),
        Input("btn-clear",       "n_clicks"),
        State("input-range",     "value"),
        State("input-azimuth",   "value"),
        State("input-elevation", "value"),
        State("targets-store",   "data"),
        State("target-counter",  "data"),
        prevent_initial_call=True,
    )
    def handle_buttons(add_clicks, clear_clicks, range_m, azimuth, elevation,
                       targets, counter):
        from dash import ctx
        if ctx.triggered_id == "btn-clear":
            return [], 0, ""

        # btn-add
        try:
            result = client.query(range_m, azimuth, elevation)
        except RuntimeError as e:
            return no_update, no_update, str(e)

        counter += 1
        result["id"]           = counter
        result["azimuth_deg"]  = azimuth
        result["elevation_deg"]= elevation
        targets.append(result)
        return targets, counter, ""

    @callback(
        Output("target-layer", "children"),
        Input("targets-store", "data"),
    )
    def update_map(targets):
        return map_view.build_target_markers(targets)

    app.run(debug=False, host="127.0.0.1", port=8050)


if __name__ == "__main__":
    main()
