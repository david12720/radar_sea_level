#include "elevation_api.h"
#include <stdio.h>

int main(void) {
    ElevationService* svc = es_create("./tiles_global/");
    if (!svc) { printf("create failed\n"); return 1; }

    static const struct { double lat; double lon; const char* name; } pts[] = {
        { 32.0853,  34.7818, "Tel Aviv" },
        { 27.9881,  86.9250, "Everest summit" },
        { 40.7128, -74.0060, "NYC" },
        {  0.0,      0.0,    "Null Island (sea)" }
    };
    int i;
    for (i = 0; i < 4; ++i) {
        double m;
        int rc = es_get_elev(svc, pts[i].lat, pts[i].lon, &m);
        if (rc == ES_OK)               printf("%-20s %.1f m\n", pts[i].name, m);
        else if (rc == ES_ERR_NO_DATA) printf("%-20s no-data\n", pts[i].name);
        else                           printf("%-20s err=%d\n",  pts[i].name, rc);
    }
    es_destroy(svc);
    return 0;
}
