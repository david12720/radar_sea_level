#include "elevation_api.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static double now_us(void)
{
    LARGE_INTEGER t, f;
    QueryPerformanceCounter(&t);
    QueryPerformanceFrequency(&f);
    return (double)t.QuadPart * 1e6 / (double)f.QuadPart;
}

static void query_and_print(ElevationService* svc, double lat, double lon)
{
    double msl = 0.0;
    double t0  = now_us();
    int    rc  = es_get_elev(svc, lat, lon, &msl);
    double dt  = now_us() - t0;

    printf("  lat=%.6f  lon=%.6f  ->  ", lat, lon);
    switch (rc) {
        case ES_OK:          printf("%.1f m MSL", msl);  break;
        case ES_ERR_NO_DATA: printf("no-data (sea or missing tile)"); break;
        case ES_ERR_BAD_ARG: printf("bad-arg (out of range)");        break;
        case ES_ERR_IO:      printf("io-error (tile unreadable)");     break;
        default:             printf("error %d", rc);                   break;
    }
    printf("   (%.2f us)\n", dt);
}

static const struct { double lat; double lon; const char* name; } PRESETS[] = {
    { 32.0853,  34.7818, "Tel Aviv"       },
    { 31.7683,  35.2137, "Jerusalem"      },
    { 32.8156,  35.1072, "Haifa"          },
    { 31.5000,  35.4500, "Dead Sea"       },
    { 30.8000,  35.0000, "Negev"          },
    { 28.5000,  34.9000, "Eilat area"     },
    { 33.5000,  36.3000, "Damascus area"  },
    {  0.0,      0.0,    "Null Island"    },
};
static const int N_PRESETS = (int)(sizeof(PRESETS) / sizeof(PRESETS[0]));

static void exe_dir(char* buf, int cap)
{
    DWORD n = GetModuleFileNameA(NULL, buf, (DWORD)cap);
    if (n == 0) { buf[0] = '.'; buf[1] = '\0'; return; }
    /* strip filename, keep trailing backslash */
    for (int i = (int)n - 1; i >= 0; --i) {
        if (buf[i] == '\\' || buf[i] == '/') { buf[i + 1] = '\0'; return; }
    }
    buf[0] = '.'; buf[1] = '\\'; buf[2] = '\0';
}

int main(int argc, char** argv)
{
    char default_dir[MAX_PATH];
    const char* tiles_dir = NULL;
    int first_coord = 1;

    /* Parse --tiles <dir> anywhere in the argument list */
    {
        int i;
        for (i = 1; i < argc; ++i) {
            if (strcmp(argv[i], "--tiles") == 0 && i + 1 < argc) {
                tiles_dir    = argv[i + 1];
                first_coord  = i + 2;
                break;
            }
        }
    }

    if (!tiles_dir) {
        /* Default: <exe_dir>\..\..\..\dted_maps  (works from x64\Release\ or Win32\Release\) */
        exe_dir(default_dir, MAX_PATH);
        strncat_s(default_dir, MAX_PATH, "..\\..\\..\\" "dted_maps", _TRUNCATE);
        tiles_dir = default_dir;
        printf("No --tiles given — using default: %s\n", tiles_dir);
    }

    printf("Loading service from: %s\n", tiles_dir);
    double t0  = now_us();
    ElevationService* svc = es_create(tiles_dir);
    double dt  = now_us() - t0;
    if (!svc) {
        printf("es_create() failed — check tiles_dir path.\n");
        printf("Usage: poc_consumer.exe [--tiles <dir>] [lat lon ...]\n");
        return 1;
    }
    printf("Service ready (%.2f us)\n\n", dt);

    /* If lat/lon pairs given on command line, query them and exit */
    if (first_coord + 1 <= argc - 1) {
        int i = first_coord;
        while (i + 1 < argc) {
            double lat = atof(argv[i]);
            double lon = atof(argv[i + 1]);
            query_and_print(svc, lat, lon);
            i += 2;
        }
        es_destroy(svc);
        return 0;
    }

    /* Preset queries */
    printf("--- Preset locations ---\n");
    for (int i = 0; i < N_PRESETS; ++i) {
        printf("  %-16s", PRESETS[i].name);
        query_and_print(svc, PRESETS[i].lat, PRESETS[i].lon);
    }
    printf("\n");

    /* Interactive loop */
    printf("--- Interactive mode (type: lat lon  |  'q' to quit) ---\n");
    char line[128];
    for (;;) {
        printf("> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;
        if (line[0] == 'q' || line[0] == 'Q') break;

        double lat, lon;
        if (sscanf_s(line, "%lf %lf", &lat, &lon) == 2) {
            query_and_print(svc, lat, lon);
        } else {
            printf("  Enter: <lat> <lon>  (e.g.  32.08 34.78)\n");
        }
    }

    es_destroy(svc);
    printf("Done.\n");
    return 0;
}
