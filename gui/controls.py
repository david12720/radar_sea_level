"""
Input panel layout — range, azimuth, elevation controls + action buttons.
Returns a Dash component; has no server or map knowledge.
"""

from dash import dcc, html

MAX_RANGE_M = 50000


def layout() -> html.Div:
    return html.Div([
        html.H3("Radar Query", style={"marginBottom": "16px"}),

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

        html.Br(),
        html.Div([
            html.Button("Add Target", id="btn-add", n_clicks=0,
                        style={"marginRight": "12px", "padding": "8px 20px",
                               "backgroundColor": "#1a73e8", "color": "white",
                               "border": "none", "borderRadius": "4px", "cursor": "pointer"}),
            html.Button("Clear All",  id="btn-clear", n_clicks=0,
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
