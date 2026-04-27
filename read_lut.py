"""
Utility to read and query the binary LUT output from lut_server.
Usage: 
    python read_lut.py <filename> <max_range_m> <az_deg> <range_m>
"""
import sys
import numpy as np

def load_lut(path, max_range_m):
    AZ_COUNT = 3600
    RNG_STEP = 15.0
    # Changed: exact count without +1 (e.g., 15000 / 15 = 1000)
    range_count = int(np.floor(max_range_m / RNG_STEP))
    
    # Read raw int32 data and reshape to 2D grid
    data = np.fromfile(path, dtype='<i4').reshape((AZ_COUNT, range_count))
    return data

def main():
    if len(sys.argv) < 5:
        print("Usage: python read_lut.py <filename> <max_range_m> <az_deg> <range_m>")
        print("Example: python read_lut.py output.bin 15000 12.5 4500")
        sys.exit(1)

    path = sys.argv[1]
    max_range = float(sys.argv[2])
    query_az = float(sys.argv[3])
    query_rng = float(sys.argv[4])

    try:
        lut = load_lut(path, max_range)
        
        # Calculate indices (0.1 deg and 15m steps)
        az_idx = int(round(query_az / 0.1)) % 3600
        rng_idx = int(round(query_rng / 15.0))
        
        # Boundary check for range
        if rng_idx >= lut.shape[1]:
            print(f"Error: Range {query_rng}m exceeds LUT capacity ({max_range}m).")
            sys.exit(1)

        elev = lut[az_idx, rng_idx]
        print(f"Elevation at {query_az} deg, {query_rng} m: {elev} m")
        
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    main()
