#include "../include/elevation_api.h"
#include "tile_store.h"
#include "bicubic.h"
#include "dt2_read.h"
#include <cmath>
#include <new>

struct ElevationService {
    TileStore store;
    explicit ElevationService(const char* tiles_dir) : store(tiles_dir) {}
};

ElevationService* es_create(const char* tiles_dir)
{
    if (!tiles_dir) return NULL;
    return new (std::nothrow) ElevationService(tiles_dir);
}

void es_destroy(ElevationService* svc)
{
    delete svc;
}

int es_get_elev(ElevationService* svc, double lat_deg, double lon_deg, double* out_msl)
{
    if (!svc || !out_msl) return ES_ERR_BAD_ARG;
    if (lat_deg < -90.0 || lat_deg > 90.0) return ES_ERR_BAD_ARG;
    if (lon_deg < -180.0 || lon_deg > 180.0) return ES_ERR_BAD_ARG;

    int lat_floor = (int)std::floor(lat_deg);
    int lon_floor = (int)std::floor(lon_deg);

    double row_f = (lat_deg - lat_floor) * (DTED_DIM - 1);
    double col_f = (lon_deg - lon_floor) * (DTED_DIM - 1);

    int r = (int)std::floor(row_f);
    int c = (int)std::floor(col_f);
    double ty = row_f - r;
    double tx = col_f - c;

    bool missing  = false;
    bool io_err   = false;
    const unsigned char* data = svc->store.get(lat_floor, lon_floor, &missing, &io_err);

    if (!data) {
        if (missing) return ES_ERR_NO_DATA;
        return ES_ERR_IO;
    }

    double p[4][4];
    for (int ci = 0; ci < 4; ++ci) {
        int cc = c + ci - 1;
        if (cc < 0) cc = 0;
        if (cc > DTED_DIM - 1) cc = DTED_DIM - 1;
        for (int ri = 0; ri < 4; ++ri) {
            int rr = r + ri - 1;
            if (rr < 0) rr = 0;
            if (rr > DTED_DIM - 1) rr = DTED_DIM - 1;
            long off = dted_post_offset(cc, rr);
            int v = decode_dted_post(read_be_i16(data + off));
            if (v == DTED_VOID) return ES_ERR_NO_DATA;
            p[ci][ri] = (double)v;
        }
    }

    *out_msl = bicubic_interpolate(p, tx, ty);
    return ES_OK;
}
