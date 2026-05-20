#include "elevation_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static double now_us(void)
{
    LARGE_INTEGER t, f;
    QueryPerformanceCounter(&t);
    QueryPerformanceFrequency(&f);
    return (double)t.QuadPart * 1e6 / (double)f.QuadPart;
}
#else
#include <time.h>

static double now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e6 + ts.tv_nsec / 1e3;
}
#endif

static int cmp_double(const void* a, const void* b)
{
    double da = *(const double*)a;
    double db = *(const double*)b;
    if (da < db) return -1;
    if (da > db) return  1;
    return 0;
}

static void run_grid(ElevationService* svc,
                     double lat_min, double lat_max,
                     double lon_min, double lon_max,
                     int steps,
                     const char* label)
{
    int n = steps * steps;
    double* times = (double*)malloc((size_t)n * sizeof(double));
    if (!times) { printf("malloc failed\n"); return; }

    int hits = 0, no_data = 0, errors = 0;
    double lat_step = (lat_max - lat_min) / (steps - 1);
    double lon_step = (lon_max - lon_min) / (steps - 1);

    for (int i = 0; i < steps; ++i) {
        double lat = lat_min + i * lat_step;
        for (int j = 0; j < steps; ++j) {
            double lon = lon_min + j * lon_step;
            double msl;
            double t0 = now_us();
            int rc = es_get_elev(svc, lat, lon, &msl);
            double t1 = now_us();
            times[i * steps + j] = t1 - t0;
            if      (rc == ES_OK)          ++hits;
            else if (rc == ES_ERR_NO_DATA) ++no_data;
            else                           ++errors;
        }
    }

    qsort(times, (size_t)n, sizeof(double), cmp_double);

    double sum = 0.0;
    for (int k = 0; k < n; ++k) sum += times[k];
    double avg  = sum / n;
    double p50  = times[(int)(n * 0.50)];
    double p95  = times[(int)(n * 0.95)];
    double p99  = times[(int)(n * 0.99)];
    double tmin = times[0];
    double tmax = times[n - 1];

    printf("--- %s  (%d x %d = %d queries) ---\n", label, steps, steps, n);
    printf("  hits=%-6d  no-data=%-6d  errors=%d\n", hits, no_data, errors);
    printf("  min=%6.2f us   avg=%6.2f us   p50=%6.2f us\n", tmin, avg, p50);
    printf("  p95=%6.2f us   p99=%6.2f us   max=%6.2f us\n", p95, p99, tmax);
    printf("  throughput: %.0f queries/sec\n", n / (sum / 1e6));
    printf("\n");

    free(times);
}

static void run_point(ElevationService* svc,
                      double lat, double lon, const char* name,
                      int iters)
{
    double* times = (double*)malloc((size_t)iters * sizeof(double));
    if (!times) { printf("malloc failed\n"); return; }

    double msl = 0.0;
    int rc_last = 0;
    for (int k = 0; k < iters; ++k) {
        double t0 = now_us();
        rc_last = es_get_elev(svc, lat, lon, &msl);
        double t1 = now_us();
        times[k] = t1 - t0;
    }

    qsort(times, (size_t)iters, sizeof(double), cmp_double);
    double sum = 0.0;
    for (int k = 0; k < iters; ++k) sum += times[k];

    const char* result_str;
    char result_buf[32];
    if (rc_last == ES_OK) {
        snprintf(result_buf, sizeof(result_buf), "%.1f m", msl);
        result_str = result_buf;
    } else if (rc_last == ES_ERR_NO_DATA) {
        result_str = "no-data";
    } else {
        result_str = "error";
    }

    printf("  %-20s elev=%-12s  min=%5.2f avg=%5.2f p99=%5.2f max=%5.2f us  (%d iters)\n",
           name, result_str,
           times[0], sum / iters,
           times[(int)(iters * 0.99)], times[iters - 1],
           iters);

    free(times);
}

int main(int argc, char** argv)
{
    const char* tiles_dir = argc > 1 ? argv[1] : "./tiles_global/";
    int grid_steps = argc > 2 ? atoi(argv[2]) : 50;
    if (grid_steps < 2) grid_steps = 2;

    printf("tiles_dir: %s\n", tiles_dir);
    printf("grid_steps: %d per axis\n\n", grid_steps);

    ElevationService* svc = es_create(tiles_dir);
    if (!svc) { printf("es_create failed\n"); return 1; }

    /* --- named-point warm-up + per-point stats --- */
    printf("=== Named points (%d iterations each, cold first call may be slow) ===\n", 1000);
    run_point(svc,  32.0853,  34.7818, "Tel Aviv",       1000);
    run_point(svc,  31.7683,  35.2137, "Jerusalem",      1000);
    run_point(svc,  32.8156,  35.1072, "Haifa",          1000);
    run_point(svc,  31.5000,  35.4500, "Dead Sea",       1000);
    run_point(svc,  30.8000,  35.0000, "Negev",          1000);
    run_point(svc,  28.5000,  34.9000, "Eilat area",     1000);
    run_point(svc,   0.0,      0.0,    "Null Island",    1000);
    printf("\n");

    /* --- grid sweeps over available coverage --- */
    /* Israel / Levant coverage: lat 28-35, lon 32-37 */
    printf("=== Grid sweep — full coverage (lat 28-35, lon 32-37) ===\n");
    run_grid(svc, 28.0, 35.0, 32.0, 37.0, grid_steps, "coverage grid");

    /* Dense grid over single tile (E034/N32 — Tel Aviv to Jerusalem) */
    printf("=== Grid sweep — single tile E034/N32 ===\n");
    run_grid(svc, 32.0, 33.0, 34.0, 35.0, grid_steps, "single tile");

    /* Worst-case: random-looking scatter across all tiles (forces LRU churn) */
    printf("=== LRU churn — cycling all tiles in sequence ===\n");
    {
        static const struct { int lat; int lon; } tile_corners[] = {
            {28,32},{29,32},{30,32},{31,32},{34,32},
            {28,33},{29,33},{30,33},{31,33},{34,33},
            {28,34},{29,34},{30,34},{31,34},{32,34},
            {28,35},{29,35},{30,35},{31,35},{32,35},{33,35},{34,35},
            {28,36},{29,36},{30,36},{31,36},{32,36},{33,36},{34,36},
        };
        int ntiles = (int)(sizeof(tile_corners)/sizeof(tile_corners[0]));
        int iters = grid_steps * grid_steps;
        double* times = (double*)malloc((size_t)iters * sizeof(double));
        if (times) {
            int hits = 0;
            for (int k = 0; k < iters; ++k) {
                int t = k % ntiles;
                double lat = tile_corners[t].lat + 0.5;
                double lon = tile_corners[t].lon + 0.5;
                double msl;
                double t0 = now_us();
                int rc = es_get_elev(svc, lat, lon, &msl);
                double t1 = now_us();
                times[k] = t1 - t0;
                if (rc == ES_OK) ++hits;
            }
            qsort(times, (size_t)iters, sizeof(double), cmp_double);
            double sum = 0.0;
            for (int k = 0; k < iters; ++k) sum += times[k];
            printf("--- LRU churn  (%d queries across %d tiles) ---\n", iters, ntiles);
            printf("  hits=%d\n", hits);
            printf("  min=%6.2f us   avg=%6.2f us   p99=%6.2f us   max=%6.2f us\n",
                   times[0], sum/iters,
                   times[(int)(iters*0.99)], times[iters-1]);
            printf("  throughput: %.0f queries/sec\n\n", iters / (sum / 1e6));
            free(times);
        }
    }

    es_destroy(svc);
    return 0;
}
