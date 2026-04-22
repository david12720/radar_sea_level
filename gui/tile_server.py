"""
Serves map tiles from a local MBTiles file (SQLite) over HTTP.
Runs as a background thread — import and call start().
"""

import sqlite3
import threading
import socket
import os
from flask import Flask, Response, abort

_app = Flask(__name__)
_db_path: str = ""
_local = threading.local()   # per-thread SQLite connection


def _free_port(preferred: int = 8081) -> int:
    for port in range(preferred, preferred + 20):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            if s.connect_ex(("127.0.0.1", port)) != 0:
                return port
    raise RuntimeError("No free port found for tile server")


def _conn() -> sqlite3.Connection:
    if not hasattr(_local, "con") or _local.con is None:
        _local.con = sqlite3.connect(_db_path, check_same_thread=False)
    return _local.con


def _get_tile(z: int, x: int, y: int) -> bytes | None:
    # MBTiles row index is TMS (origin bottom-left); Leaflet uses XYZ (origin top-left)
    tms_y = (1 << z) - 1 - y
    try:
        row = _conn().execute(
            "SELECT tile_data FROM tiles WHERE zoom_level=? AND tile_column=? AND tile_row=?",
            (z, x, tms_y)
        ).fetchone()
        return bytes(row[0]) if row else None
    except sqlite3.Error:
        _local.con = None   # reset on error, will reconnect next request
        return None


@_app.route("/tiles/<int:z>/<int:x>/<int:y>.png")
def serve_tile(z: int, x: int, y: int):
    data = _get_tile(z, x, y)
    if data is None:
        abort(404)
    return Response(data, mimetype="image/png")


@_app.route("/tiles/health")
def health():
    return {"status": "ok"}


def start(mbtiles_path: str, host: str = "127.0.0.1") -> str:
    global _db_path
    _db_path = os.path.abspath(mbtiles_path)   # resolve once, always absolute
    port = _free_port()
    thread = threading.Thread(
        target=lambda: _app.run(host="0.0.0.0", port=port, debug=False, use_reloader=False),
        daemon=True
    )
    thread.start()
    return f"http://{host}:{port}/tiles/{{z}}/{{x}}/{{y}}.png"
