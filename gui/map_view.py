"""
Dash-Leaflet map component.
Renders radar marker, max-range circle, and target pins color-coded by AGL.
"""

import dash_leaflet as dl
from dash import html

RADAR_ZOOM         = 10
RANGE_CIRCLE_COLOR = "#1a73e8"
DEFAULT_CENTER     = [32.08, 34.76]


def _agl_color(agl_m: float) -> str:
    if agl_m < 50:    return "red"
    if agl_m < 300:   return "orange"
    if agl_m < 1000:  return "yellow"
    return "green"


def _target_popup(t: dict) -> html.Div:
    def row(label, value):
        return html.Tr([
            html.Td(label, style={"color": "#666", "paddingRight": "10px", "fontSize": "12px"}),
            html.Td(value, style={"fontWeight": "bold", "fontSize": "12px"}),
        ])

    return html.Div([
        html.Div(f"Target #{t['id']}", style={
            "fontWeight": "bold", "fontSize": "14px",
            "marginBottom": "6px", "borderBottom": "1px solid #ddd", "paddingBottom": "4px"
        }),
        html.Table([
            row("Lat / Lon",  f"{t['lat_deg']:.5f}° / {t['lon_deg']:.5f}°"),
            row("Alt MSL",    f"{t['alt_msl_m']:.1f} m"),
            row("Ground",     f"{t['ground_elev_m']:.1f} m"),
            row("AGL",        f"{t['agl_m']:.1f} m"),
            row("Range",      f"{t['horiz_range_m']:.0f} m"),
            row("Az / El",    f"{t['azimuth_deg']:.1f}° / {t['elevation_deg']:.1f}°"),
            row("Rel El",     f"{t['relative_elev_deg']:.2f}°"),
            *([ row("Open Elev ⟂",
                    f"{t['open_elevation_m']:.1f} m"
                    if t.get('open_elevation_m') is not None
                    else "N/A") ]
              if "open_elevation_m" in t else []),
        ], style={"borderCollapse": "collapse"}),
    ], style={"minWidth": "220px"})


def _radar_popup(r: dict) -> html.Div:
    def row(label, value):
        return html.Tr([
            html.Td(label, style={"color": "#666", "paddingRight": "10px", "fontSize": "12px"}),
            html.Td(value, style={"fontWeight": "bold", "fontSize": "12px"}),
        ])
    return html.Div([
        html.Div("Radar", style={
            "fontWeight": "bold", "fontSize": "14px",
            "marginBottom": "6px", "borderBottom": "1px solid #ddd", "paddingBottom": "4px"
        }),
        html.Table([
            row("Lat",        f"{r['lat_deg']:.5f}°"),
            row("Lon",        f"{r['lon_deg']:.5f}°"),
            row("Ground",     f"{r.get('ground_elev_m', 0):.1f} m MSL"),
            row("Height AGL", f"{r.get('agl_m', 0):.1f} m"),
            row("Alt MSL",    f"{r['alt_m']:.1f} m"),
        ], style={"borderCollapse": "collapse"}),
    ], style={"minWidth": "180px"})


def build_radar_layer(radar: dict) -> list:
    """Returns [marker, circle] for the radar-layer LayerGroup."""
    if not radar:
        return []
    lat = radar["lat_deg"]
    lon = radar["lon_deg"]
    max_range_m = radar.get("max_range_m", 50000)
    return [
        dl.Marker(
            position=[lat, lon],
            children=[
                dl.Tooltip("Radar"),
                dl.Popup(_radar_popup(radar), maxWidth=200),
            ],
            icon={
                "iconUrl": "https://raw.githubusercontent.com/pointhi/leaflet-color-markers/master/img/marker-icon-blue.png",
                "iconSize":   [25, 41],
                "iconAnchor": [12, 41],
            }
        ),
        dl.Circle(
            center=[lat, lon],
            radius=max_range_m,
            color=RANGE_CIRCLE_COLOR,
            fill=True,
            fillOpacity=0.04,
            weight=1,
        ),
    ]


def layout(initial_radar: dict, tile_url: str, max_native_zoom: int = 16) -> html.Div:
    center = ([initial_radar["lat_deg"], initial_radar["lon_deg"]]
              if initial_radar else DEFAULT_CENTER)

    elevation_bar = html.Div(
        id="elevation-bar",
        children="Click on the map to see elevation",
        style={
            "position":      "absolute",
            "bottom":        "24px",
            "left":          "50%",
            "transform":     "translateX(-50%)",
            "zIndex":        1000,
            "background":    "rgba(255,255,255,0.88)",
            "padding":       "4px 12px",
            "borderRadius":  "4px",
            "fontSize":      "12px",
            "fontFamily":    "monospace",
            "boxShadow":     "0 1px 4px rgba(0,0,0,0.3)",
            "pointerEvents": "none",
        },
    )

    return html.Div(
        [
            dl.Map(
                id="map",
                center=center,
                zoom=RADAR_ZOOM,
                children=[
                    dl.TileLayer(url=tile_url, attribution="© OpenStreetMap contributors",
                                 maxNativeZoom=max_native_zoom, maxZoom=max_native_zoom + 4),
                    dl.LayerGroup(id="radar-layer",
                                  children=build_radar_layer(initial_radar)),
                    dl.LayerGroup(id="target-layer"),
                ],
                style={"width": "100%", "height": "100%"},
            ),
            elevation_bar,
        ],
        style={"flex": 1, "height": "100vh", "position": "relative"},
    )


def build_target_markers(targets: list[dict]) -> list:
    markers = []
    for t in targets:
        color = _agl_color(t["agl_m"])
        markers.append(
            dl.CircleMarker(
                center=[t["lat_deg"], t["lon_deg"]],
                radius=8,
                color=color,
                fillColor=color,
                fillOpacity=0.85,
                children=dl.Popup(_target_popup(t), maxWidth=220),
            )
        )
    return markers
