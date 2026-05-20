#ifndef DT2_READ_H
#define DT2_READ_H

#include <stdio.h>

static const long DTED_HEADER     = 3428;
static const long DTED_REC_HEADER = 8;
static const long DTED_REC_FOOTER = 4;
static const int  DTED_DIM        = 3601;
static const long DTED_COL_STRIDE = 7214;
static const long DTED_FILE_SIZE  = 25981042;
static const int  DTED_VOID       = -32768;

static const int TILES_DIR_MAX = 512;

static inline long dted_post_offset(int c, int r)
{
    return DTED_HEADER
         + (long)c * DTED_COL_STRIDE
         + DTED_REC_HEADER
         + (long)r * 2;
}

static inline short read_be_i16(const unsigned char* p)
{
    return (short)((unsigned short)((p[0] << 8) | p[1]));
}

/* DTED2 uses signed-magnitude encoding for negative elevations.
   Void sentinel 0x8000 (-32768 two's-complement) is unchanged.
   All other negative raw values: actual elevation = -(raw & 0x7FFF). */
static inline int decode_dted_post(short raw)
{
    if (raw == (short)DTED_VOID) return DTED_VOID;
    if (raw < 0)                 return -(raw & 0x7FFF);
    return (int)raw;
}

static inline int build_tile_path(const char* tiles_dir, int lat_floor, int lon_floor,
                                  char* out, int out_cap)
{
    char lon_dir[16];
    char lat_file[16];
    int abs_lon, abs_lat;
    int n;

    abs_lon = lon_floor < 0 ? -lon_floor : lon_floor;
    abs_lat = lat_floor < 0 ? -lat_floor : lat_floor;

    if (lon_floor >= 0)
        snprintf(lon_dir, sizeof(lon_dir), "e%03d", abs_lon);
    else
        snprintf(lon_dir, sizeof(lon_dir), "w%03d", abs_lon);

    if (lat_floor >= 0)
        snprintf(lat_file, sizeof(lat_file), "n%02d", abs_lat);
    else
        snprintf(lat_file, sizeof(lat_file), "s%02d", abs_lat);

    n = snprintf(out, (size_t)out_cap, "%s/%s/%s.dt2", tiles_dir, lon_dir, lat_file);
    return n > 0 && n < out_cap;
}

#endif /* DT2_READ_H */
