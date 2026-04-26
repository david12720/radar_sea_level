"""
Input panel layout — radar position form + target query controls.
Returns a Dash component; has no server or map knowledge.
"""

from dash import dcc, html

MAX_RANGE_M = 50000


def layout(online_mode: bool = False) -> html.Div:
    """Constructs the sidebar control panel with forms for Radar, Target, and Coordinate Conversion."""
    return html.Div([

        # ── Radar position ────────────────────────────────────────────────────
        html.H3("Radar Position", style={"marginBottom": "12px"}),

        html.Label("Latitude (°)"),
        dcc.Input(id="input-lat", type="text", value="31.860342182745416",
                  debounce=True,
                  style={"width": "100%", "marginBottom": "8px", "padding": "4px"}),

        html.Label("Longitude (°)"),
        dcc.Input(id="input-lon", type="text", value="34.91973316662749",
                  debounce=True,
                  style={"width": "100%", "marginBottom": "8px", "padding": "4px"}),

        html.Label("Height above ground (m)"),
        dcc.Input(id="input-alt-msl", type="number", value=0,
                  debounce=True, step=1,
                  style={"width": "100%", "marginBottom": "10px", "padding": "4px"}),

        html.Button("Set Radar", id="btn-set-radar", n_clicks=0,
                    style={"width": "100%", "padding": "8px",
                           "backgroundColor": "#34a853", "color": "white",
                           "border": "none", "borderRadius": "4px", "cursor": "pointer"}),

        html.Div(id="radar-status-msg",
                 style={"marginTop": "6px", "fontSize": "12px", "minHeight": "18px"}),

        html.Hr(style={"margin": "16px 0"}),

        # ── Target query ──────────────────────────────────────────────────────
        html.H3("Target Query", style={"marginBottom": "16px"}),

        html.Label("Range (m)"),
        dcc.Slider(
            id="input-range",
            min=0, max=MAX_RANGE_M, step=15,
            value=5000,
            marks={0: "0", 10000: "10k", 25000: "25k", 50000: "50k"},
            tooltip={"placement": "bottom", "always_visible": True},
        ),

        html.Br(),
        html.Label("Azimuth (°, from North)"),
        dcc.Slider(
            id="input-azimuth",
            min=0, max=359.9, step=0.1,
            value=0,
            marks={0: "N", 90: "E", 180: "S", 270: "W"},
            tooltip={"placement": "bottom", "always_visible": True},
        ),

        html.Br(),
        html.Label("Elevation (°)"),
        dcc.Slider(
            id="input-elevation",
            min=-10, max=45, step=0.1,
            value=0,
            marks={-10: "-10°", 0: "0°", 20: "20°", 45: "45°"},
            tooltip={"placement": "bottom", "always_visible": True},
        ),

        html.Div([
            dcc.Checklist(
                id="chk-open-elevation",
                options=[{
                    "label": " Use Open Elevation API",
                    "value": "on",
                    "disabled": not online_mode,
                }],
                value=[],
                style={"fontSize": "13px",
                       "color": "#333" if online_mode else "#aaa"},
                inputStyle={"cursor": "pointer" if online_mode else "not-allowed"},
            ),
            html.Span(
                "● Online" if online_mode else "● Offline",
                style={"fontSize": "11px", "marginLeft": "6px",
                       "color": "green" if online_mode else "#aaa"},
            ),
        ], style={"marginTop": "16px", "marginBottom": "10px", "display": "flex", "alignItems": "center"}),

        html.Label("Earth model"),
        dcc.Dropdown(
            id="input-earth-model",
            options=[
                {"label": "Flat (fast, <50 km)",           "value": "flat"},
                {"label": "Spherical (R = 6371 km)",       "value": "sphere"},
                {"label": "Refractive 4/3 (atmospheric)",  "value": "k43"},
            ],
            value="flat", clearable=False,
            style={"fontSize": "13px", "marginBottom": "12px"},
        ),

        html.Div([
            html.Button("Add Target", id="btn-add", n_clicks=0,
                        style={"marginRight": "12px", "padding": "8px 20px",
                               "backgroundColor": "#1a73e8", "color": "white",
                               "border": "none", "borderRadius": "4px", "cursor": "pointer"}),
            html.Button("Clear All", id="btn-clear", n_clicks=0,
                        style={"padding": "8px 20px", "backgroundColor": "#d93025",
                               "color": "white", "border": "none",
                               "borderRadius": "4px", "cursor": "pointer"}),
        ]),

        html.Br(),
        html.Div(id="error-msg",
                 style={"color": "red", "fontWeight": "bold", "minHeight": "24px"}),

        html.Hr(style={"margin": "16px 0"}),

        # ── Coordinate converter ──────────────────────────────────────────────
        html.H3("Coordinate Converter", style={"marginBottom": "12px"}),

        # ── LL → UTM ──────────────────────────────────────────────────────────
        html.Div("Lat/Lon → UTM (WGS84)", style={"fontWeight": "bold",
                                                   "fontSize": "13px",
                                                   "marginBottom": "6px"}),
        html.Label("Latitude (°)"),
        dcc.Input(id="conv-lat", type="text", placeholder="e.g. 32.0",
                  debounce=True,
                  style={"width": "100%", "marginBottom": "6px", "padding": "4px"}),
        html.Label("Longitude (°)"),
        dcc.Input(id="conv-lon", type="text", placeholder="e.g. 35.0",
                  debounce=True,
                  style={"width": "100%", "marginBottom": "8px", "padding": "4px"}),
        html.Button("Convert →", id="btn-ll-to-utm", n_clicks=0,
                    style={"width": "100%", "padding": "6px",
                           "backgroundColor": "#5f6368", "color": "white",
                           "border": "none", "borderRadius": "4px", "cursor": "pointer",
                           "marginBottom": "6px"}),
        html.Div(id="conv-ll-to-utm-result",
                 style={"fontSize": "12px", "minHeight": "18px", "marginBottom": "12px",
                        "fontFamily": "monospace", "wordBreak": "break-all"}),

        # ── UTM → LL ──────────────────────────────────────────────────────────
        html.Div("UTM → Lat/Lon (WGS84)", style={"fontWeight": "bold",
                                                   "fontSize": "13px",
                                                   "marginBottom": "6px"}),
        html.Div([
            html.Div([
                html.Label("Easting (m)"),
                dcc.Input(id="conv-easting", type="number", placeholder="e.g. 717000",
                          debounce=True,
                          style={"width": "100%", "padding": "4px"}),
            ], style={"flex": "1", "marginRight": "6px"}),
            html.Div([
                html.Label("Northing (m)"),
                dcc.Input(id="conv-northing", type="number", placeholder="e.g. 3545000",
                          debounce=True,
                          style={"width": "100%", "padding": "4px"}),
            ], style={"flex": "1"}),
        ], style={"display": "flex", "marginBottom": "6px"}),
        html.Div([
            html.Div([
                html.Label("Zone"),
                dcc.Input(id="conv-zone", type="number", placeholder="36",
                          debounce=True, min=1, max=60, step=1,
                          style={"width": "100%", "padding": "4px"}),
            ], style={"flex": "1", "marginRight": "6px"}),
            html.Div([
                html.Label("Hemisphere"),
                dcc.Dropdown(id="conv-hemisphere",
                             options=[{"label": "N", "value": "N"},
                                      {"label": "S", "value": "S"}],
                             value="N", clearable=False,
                             style={"fontSize": "13px"}),
            ], style={"flex": "1"}),
        ], style={"display": "flex", "marginBottom": "8px"}),
        html.Button("Convert →", id="btn-utm-to-ll", n_clicks=0,
                    style={"width": "100%", "padding": "6px",
                           "backgroundColor": "#5f6368", "color": "white",
                           "border": "none", "borderRadius": "4px", "cursor": "pointer",
                           "marginBottom": "6px"}),
        html.Div(id="conv-utm-to-ll-result",
                 style={"fontSize": "12px", "minHeight": "18px",
                        "fontFamily": "monospace", "wordBreak": "break-all"}),

    ], style={"padding": "20px", "width": "320px", "flexShrink": 0,
              "fontFamily": "sans-serif", "backgroundColor": "#f8f9fa",
              "borderRight": "1px solid #ddd", "overflowY": "auto"})
