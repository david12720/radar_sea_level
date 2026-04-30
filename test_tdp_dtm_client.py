"""
TCP client for testing tdp_dtm_server.

Sends a UTM radar position and receives terrain MSL + a saved output.bin.

Usage:
    python test_tdp_dtm_client.py [--host 127.0.0.1] [--port 9001]
                                  [--easting 717000] [--northing 3527000] [--zone 36]

Wire protocol
    Request  : BlkHdr (5 x uint32 = 20 bytes) + x,y,z (3 x float32 = 12 bytes)
    Response : validity (int32) + dted_height (float32) + spare[3] (3 x float32)
"""

import argparse
import socket
import struct
import sys


# ── protocol constants ────────────────────────────────────────────────────────
REQUEST_FMT  = "<5I3f"   # blk_hdr (5 uint32) + x, y, z (3 float32)
REQUEST_SIZE = struct.calcsize(REQUEST_FMT)   # 32 bytes

RESPONSE_FMT  = "<i4f"  # validity (int32) + dted_height + spare[3] (4 float32)
RESPONSE_SIZE = struct.calcsize(RESPONSE_FMT) # 20 bytes


def _recvall(s: socket.socket, n: int) -> bytes:
    buf = bytearray()
    while len(buf) < n:
        chunk = s.recv(n - len(buf))
        if not chunk:
            raise ConnectionError(f"Connection closed after {len(buf)}/{n} bytes")
        buf.extend(chunk)
    return bytes(buf)


def send_request(host: str, port: int,
                 easting: float, northing: float, zone: int) -> None:

    # blk_hdr fields are zero — server ignores them
    payload = struct.pack(REQUEST_FMT,
                          0, 0, 0, 0, 0,          # blk_hdr
                          easting, northing, float(zone))

    print(f"Connecting to {host}:{port} ...")
    with socket.create_connection((host, port), timeout=120) as s:
        s.sendall(payload)
        print(f"Request sent  easting={easting:.1f}  northing={northing:.1f}  zone={zone}")
        print("Waiting for response ...")

        raw = _recvall(s, RESPONSE_SIZE)

    validity, dted_height, *spare = struct.unpack(RESPONSE_FMT, raw)

    print(f"\n── Response ────────────────────────────────────────────────")
    if validity == 0:
        print(f"  Status      : OK")
        print(f"  Terrain MSL : {dted_height:.2f} m")
        print(f"  output.bin  : saved by server")
    else:
        print(f"  Status      : FAILED (validity={validity})", file=sys.stderr)
        sys.exit(1)


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--host",      default="127.0.0.1")
    p.add_argument("--port",      type=int,   default=9001)
    p.add_argument("--easting",   type=float, default=717000.0,
                   help="UTM easting in meters")
    p.add_argument("--northing",  type=float, default=3527000.0,
                   help="UTM northing in meters")
    p.add_argument("--zone",      type=int,   default=36,
                   help="UTM zone number (1-60)")
    args = p.parse_args()

    send_request(args.host, args.port, args.easting, args.northing, args.zone)


if __name__ == "__main__":
    main()
