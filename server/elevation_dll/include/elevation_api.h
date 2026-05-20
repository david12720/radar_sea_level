#ifndef ELEVATION_API_H
#define ELEVATION_API_H

#if defined(_WIN32)
  #if defined(ELEVATION_DLL_EXPORTS)
    #define ELEV_API __declspec(dllexport)
  #else
    #define ELEV_API __declspec(dllimport)
  #endif
  #define ELEV_CALL __cdecl
#else
  #if defined(ELEVATION_DLL_EXPORTS) && (defined(__GNUC__) || defined(__clang__))
    #define ELEV_API __attribute__((visibility("default")))
  #else
    #define ELEV_API
  #endif
  #define ELEV_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ElevationService ElevationService;

#define ES_OK             0
#define ES_ERR_BAD_ARG   -1
#define ES_ERR_NO_DATA   -2
#define ES_ERR_IO        -3

ELEV_API ElevationService* ELEV_CALL es_create(const char* tiles_dir);
ELEV_API void              ELEV_CALL es_destroy(ElevationService* svc);
ELEV_API int               ELEV_CALL es_get_elev(ElevationService* svc,
                                                  double lat_deg, double lon_deg,
                                                  double* out_msl);

#ifdef __cplusplus
}
#endif

#endif /* ELEVATION_API_H */
