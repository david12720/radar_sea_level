"""
Dash-Leaflet map component.
Renders radar marker, max-range circle, and target pins color-coded by AGL.
"""

import math
import dash_leaflet as dl
from dash import html

RADAR_ZOOM   = 10
RANGE_CIRCLE_COLOR = "#1a73e8"


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
        ], style={"borderCollapse": "collapse"}),
    ], style={"minWidth": "220px"})


def _radar_popup(lat: float, lon: float, alt: float) -> html.Div:
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
            row("Lat",     f"{lat:.5f}°"),
            row("Lon",     f"{lon:.5f}°"),
            row("Alt MSL", f"{alt:.1f} m"),
        ], style={"borderCollapse": "collapse"}),
    ], style={"minWidth": "160px"})


def layout(radar_lat: float, radar_lon: float, radar_alt: float,
           max_range_m: float, tile_url: str) -> html.Div:
    radar_marker = dl.Marker(
        position=[radar_lat, radar_lon],
        children=[
            dl.Tooltip("Radar"),
            dl.Popup(_radar_popup(radar_lat, radar_lon, radar_alt), maxWidth=180),
        ],
        icon={
            "iconUrl": "https://raw.githubusercontent.com/pointhi/leaflet-color-markers/master/img/marker-icon-blue.png",
            "iconSize":   [25, 41],
            "iconAnchor": [12, 41],
        }
    )

    range_circle = dl.Circle(
        center=[radar_lat, radar_lon],
        radius=max_range_m,
        color=RANGE_CIRCLE_COLOR,
        fill=True,
        fillOpacity=0.04,
        weight=1,
    )

    return html.Div(
        dl.Map(
            id="map",
            center=[radar_lat, radar_lon],
            zoom=RADAR_ZOOM,
            children=[
                dl.TileLayer(url=tile_url, attribution="© OpenStreetMap contributors"),
                range_circle,
                radar_marker,
                dl.LayerGroup(id="target-layer"),
            ],
            style={"width": "100%", "height": "100%"},
        ),
        style={"flex": 1, "height": "100vh"},
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
