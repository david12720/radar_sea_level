"""
Input panel layout — radar position form + target query controls.
Returns a Dash component; has no server or map knowledge.
"""

from dash import dcc, html

MAX_RANGE_M = 50000


def layout(online_mode: bool = False) -> html.Div:
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

        html.Label("Altitude MSL (m)"),
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
                options=[{"label": " Use Open Elevation API", "value": "on"}],
                value=[],
                style={"fontSize": "13px",
                       "color": "#333" if online_mode else "#aaa"},
                inputStyle={"cursor": "pointer" if online_mode else "not-allowed"},
                **({"disabled": True} if not online_mode else {}),
            ),
            html.Span(
                "● Online" if online_mode else "● Offline",
                style={"fontSize": "11px", "marginLeft": "6px",
                       "color": "green" if online_mode else "#aaa"},
            ),
        ], style={"marginBottom": "10px", "display": "flex", "alignItems": "center"}),

        html.Br(),
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

    ], style={"padding": "20px", "width": "320px", "flexShrink": 0,
              "fontFamily": "sans-serif", "backgroundColor": "#f8f9fa",
              "borderRight": "1px solid #ddd", "overflowY": "auto"})
