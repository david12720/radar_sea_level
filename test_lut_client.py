"""
TCP client for testing lut_server.

Usage:
    python test_lut_client.py [--host 127.0.0.1] [--port 9000]
                              [--lat 32.08] [--lon 34.76] [--alt-agl 10]
                              [--max-range 15000]
                              [--save output.bin]

Without --save: receives the array over TCP and prints a summary.
With --save:    asks the server to write the file directly (server-side path).
"""

import argparse
import socket
import struct
import sys


def send_request(host: str, port: int,
                 lat: float, lon: float, alt: float, max_range: float,
                 save_path: str | None) -> None:

    mode = 1 if save_path else 0

    # Fixed header: 4 doubles + 1 uint8 = 33 bytes
    header = struct.pack("<4dB", lat, lon, alt, max_range, mode)

    if mode == 1:
        encoded = save_path.encode()
        header += struct.pack("<H", len(encoded)) + encoded

    print(f"Connecting to {host}:{port} ...")
    with socket.create_connection((host, port), timeout=120) as s:
        s.sendall(header)
        print(f"Request sent  lat={lat} lon={lon} alt_agl={alt}m max_range={max_range}m  mode={'save' if mode else 'receive'}")

        if mode == 0:
            _receive_and_print(s, max_range)
        else:
            _receive_status(s, save_path)


def _recvall(s: socket.socket, n: int) -> bytes:
    buf = bytearray()
    while len(buf) < n:
        chunk = s.recv(n - len(buf))
        if not chunk:
            raise ConnectionError(f"Connection closed after {len(buf)}/{n} bytes")
        buf.extend(chunk)
    return bytes(buf)


def _receive_and_print(s: socket.socket, max_range: float) -> None:
    print("Waiting for array dimensions ...")
    dims = _recvall(s, 8)
    az_count, range_count = struct.unpack("<2I", dims)
    total_cells = az_count * range_count
    total_bytes = total_cells * 4

    print(f"Array shape : int32[{az_count}][{range_count}]  ({total_bytes / 1024 / 1024:.1f} MB)")
    print(f"Receiving {total_cells:,} int32 values ...")

    raw = _recvall(s, total_bytes)
    cells = struct.unpack(f"<{total_cells}i", raw)

    # ── Spot checks ──────────────────────────────────────────────────────────
    print("\n── Spot checks ─────────────────────────────────────────────")

    def elev(az_idx: int, range_idx: int) -> int:
        return cells[az_idx * range_count + range_idx]

    checks = [
        (0,   0,   "az=0.0°  range=0m    (radar position)"),
        (12,  100, "az=1.2°  range=1500m"),
        (900, 200, "az=90.0° range=3000m (East)"),
        (az_count - 1, range_count - 1, f"az={((az_count-1)*0.1):.1f}°  range={(range_count-1)*15}m (last cell)"),
    ]
    for ai, ri, label in checks:
        if ai < az_count and ri < range_count:
            print(f"  [{ai:4d}][{ri:4d}]  {label:45s}  elev={elev(ai, ri):6d} m")

    # ── Statistics ───────────────────────────────────────────────────────────
    non_zero = sum(1 for v in cells if v != 0)
    min_elev = min(cells)
    max_elev = max(cells)
    print(f"\n── Statistics ──────────────────────────────────────────────")
    print(f"  Min elev : {min_elev} m")
    print(f"  Max elev : {max_elev} m")
    print(f"  Non-zero : {non_zero:,} / {total_cells:,} cells ({100*non_zero/total_cells:.1f}%)")
    print("\nOK")


def _receive_status(s: socket.socket, save_path: str) -> None:
    status_byte = _recvall(s, 1)
    status = struct.unpack("B", status_byte)[0]
    if status == 0:
        print(f"Server saved file: {save_path}  OK")
    else:
        print(f"Server reported error saving: {save_path}", file=sys.stderr)
        sys.exit(1)


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--host",      default="127.0.0.1")
    p.add_argument("--port",      type=int,   default=9000)
    p.add_argument("--lat",       type=float, default=31.860342182745416)
    p.add_argument("--lon",       type=float, default=34.91973316662749)
    p.add_argument("--alt-agl",   type=float, default=0.0,
                   help="Radar height above local ground in meters (AGL)")
    p.add_argument("--max-range", type=float, default=15000.0)
    p.add_argument("--save",      default=None,
                   metavar="PATH", help="Ask server to save binary file at this path")
    args = p.parse_args()

    send_request(args.host, args.port,
                 args.lat, args.lon, args.alt_agl, args.max_range,
                 args.save)


if __name__ == "__main__":
    main()
