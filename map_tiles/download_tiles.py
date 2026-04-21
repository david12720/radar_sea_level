"""
Fast parallel tile downloader — saves to MBTiles (SQLite).
Downloads OSM tiles for the Israel bounding box at z0-16.

Run from project root:
    python map_tiles/download_tiles.py

Supports resume: re-running skips tiles already in the database.
Respects OSM tile usage policy: 8 parallel workers.
"""

import argparse
import sqlite3
import math
import os
import threading
import concurrent.futures
import urllib.request

OUTPUT     = "map_tiles/israel.mbtiles"
TILE_URL   = "https://tile.openstreetmap.org/{z}/{x}/{y}.png"
USER_AGENT = "radar-sea-level-app/1.0 (offline map for private radar system)"

BBOX_W, BBOX_S, BBOX_E, BBOX_N = 33.5, 29.0, 37.0, 34.5
MIN_ZOOM   = 0
MAX_ZOOM   = 16
WORKERS    = 8
DB_BATCH   = 500   # rows per SQLite commit


def _lon_to_x(lon: float, z: int) -> int:
    return int((lon + 180) / 360 * (1 << z))


def _lat_to_y(lat: float, z: int) -> int:
    lat_r = math.radians(lat)
    return int((1 - math.log(math.tan(lat_r) + 1 / math.cos(lat_r)) / math.pi) / 2 * (1 << z))


def _tiles_for_zoom(z: int, w: float, s: float, e: float, n: float) -> list[tuple[int, int, int]]:
    x_min = _lon_to_x(w, z)
    x_max = _lon_to_x(e, z)
    y_min = _lat_to_y(n, z)   # north = smaller y in XYZ
    y_max = _lat_to_y(s, z)
    return [(z, x, y) for x in range(x_min, x_max + 1) for y in range(y_min, y_max + 1)]


def _download(z: int, x: int, y: int) -> tuple[int, int, int, bytes | None]:
    url = TILE_URL.format(z=z, x=x, y=y)
    req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    try:
        with urllib.request.urlopen(req, timeout=10) as resp:
            return z, x, y, resp.read()
    except Exception:
        return z, x, y, None


def _xyz_to_tms_y(y: int, z: int) -> int:
    return (1 << z) - 1 - y


def _init_db(con: sqlite3.Connection) -> None:
    con.execute("PRAGMA journal_mode=WAL")
    con.execute("""CREATE TABLE IF NOT EXISTS tiles (
        zoom_level INTEGER, tile_column INTEGER, tile_row INTEGER, tile_data BLOB,
        PRIMARY KEY (zoom_level, tile_column, tile_row))""")
    con.execute("""CREATE TABLE IF NOT EXISTS metadata (name TEXT PRIMARY KEY, value TEXT)""")
    con.executemany("INSERT OR IGNORE INTO metadata VALUES (?,?)", [
        ("name",        "Israel"),
        ("type",        "overlay"),
        ("version",     "1"),
        ("description", "OSM tiles for Israel region"),
        ("format",      "png"),
        ("bounds",      f"{bbox_w},{bbox_s},{bbox_e},{bbox_n}"),
    ])
    con.commit()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--max-zoom", type=int, default=MAX_ZOOM,
                        help=f"Maximum zoom level to download (default: {MAX_ZOOM})")
    parser.add_argument("--bbox", type=float, nargs=4,
                        metavar=("WEST", "SOUTH", "EAST", "NORTH"),
                        default=[BBOX_W, BBOX_S, BBOX_E, BBOX_N],
                        help="Bounding box as: west south east north (default: Israel)")
    args = parser.parse_args()
    max_zoom = args.max_zoom
    bbox_w, bbox_s, bbox_e, bbox_n = args.bbox

    all_tiles = []
    for z in range(MIN_ZOOM, max_zoom + 1):
        tiles = _tiles_for_zoom(z, bbox_w, bbox_s, bbox_e, bbox_n)
        all_tiles.extend(tiles)
        print(f"  z{z}: {len(tiles)} tiles")

    total = len(all_tiles)
    print(f"\nTotal tiles: {total}")
    print(f"Output:      {OUTPUT}\n")

    os.makedirs(os.path.dirname(OUTPUT), exist_ok=True)

    con = sqlite3.connect(OUTPUT)
    _init_db(con)

    # Resume: load set of already-downloaded (z, x, tms_y) keys
    existing = set(con.execute(
        "SELECT zoom_level, tile_column, tile_row FROM tiles").fetchall())
    pending = [
        (z, x, y) for z, x, y in all_tiles
        if (z, x, _xyz_to_tms_y(y, z)) not in existing
    ]
    skipped = total - len(pending)
    if skipped:
        print(f"Resuming — skipping {skipped} already-downloaded tiles.")
    print(f"Tiles to download: {len(pending)}\n")

    if not pending:
        print("Nothing to do.")
        con.close()
        return

    lock = threading.Lock()
    done = [0]
    failed = [0]
    write_buf: list = []

    def flush(buf: list) -> None:
        con.executemany("INSERT OR REPLACE INTO tiles VALUES (?,?,?,?)", buf)
        con.commit()
        buf.clear()

    with concurrent.futures.ThreadPoolExecutor(max_workers=WORKERS) as ex:
        futures = {ex.submit(_download, z, x, y): (z, x, y) for z, x, y in pending}
        for fut in concurrent.futures.as_completed(futures):
            z, x, y, data = fut.result()
            with lock:
                done[0] += 1
                if data:
                    write_buf.append((z, x, _xyz_to_tms_y(y, z), data))
                else:
                    failed[0] += 1
                if len(write_buf) >= DB_BATCH:
                    flush(write_buf)
                pct = (skipped + done[0]) / total * 100
                print(f"\r  {skipped + done[0]}/{total} ({pct:.1f}%)  "
                      f"downloaded={done[0]}  failed={failed[0]}", end="", flush=True)

    with lock:
        if write_buf:
            flush(write_buf)

    con.close()
    size_mb = os.path.getsize(OUTPUT) / 1e6
    print(f"\n\nDone — {OUTPUT} ({size_mb:.1f} MB), {failed[0]} tiles failed.")


if __name__ == "__main__":
    main()
