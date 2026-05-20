# Quick Start

## Step 1 — Build the library

**Windows (Visual Studio 2022)**

Open `server/radar_sea_level.sln`, set configuration to **Release | x64** (or
**Release | Win32** for 32-bit), and build the `elevation_dll` project. Outputs:

```
server/elevation_dll/x64/Release/elevation_dll.dll   ← runtime
server/elevation_dll/x64/Release/elevation_dll.lib   ← import lib
```

**Linux**

```bash
cd server/elevation_dll
make           # produces libelevation.so
```

---

## Step 2 — Arrange your DTED2 tiles

Tiles must follow the NGA directory layout:

```
<tiles_dir>/
  e034/          ← lowercase e/w + 3-digit zero-padded longitude floor
    n32.dt2      ← lowercase n/s + 2-digit zero-padded latitude floor
    n33.dt2
  e035/
    n32.dt2
  ...
```

Pass the root directory (the one containing `e???/` subdirectories) to
`es_create()`.

> **Windows**: NTFS is case-insensitive, so uppercase names (`E034/N32.dt2`)
> also work. On Linux the names must be lowercase.

The tiles path is set at runtime via `es_create()` — there is no config file or
environment variable. To switch datasets call `es_destroy()` and then
`es_create()` with the new path.

---

## Step 3 — Link and call

### C / C++

```c
#include "elevation_api.h"   // copy from server/elevation_dll/include/

// Windows: link against elevation_dll.lib; put elevation_dll.dll next to the exe
// Linux:   compile with -L<dir> -lelevation

ElevationService* svc = es_create("/data/dted_maps/");
if (!svc) { /* tiles_dir not found or path too long */ }

double msl;
int rc = es_get_elev(svc, 32.0853, 34.7818, &msl);  // Tel Aviv
if (rc == ES_OK) printf("%.1f m\n", msl);            // → ~20 m

es_destroy(svc);
```

### Python (ctypes)

```python
import ctypes, sys

lib = ctypes.CDLL("elevation_dll.dll" if sys.platform == "win32"
                  else "./libelevation.so")

lib.es_create.restype  = ctypes.c_void_p
lib.es_create.argtypes = [ctypes.c_char_p]
lib.es_get_elev.restype  = ctypes.c_int
lib.es_get_elev.argtypes = [ctypes.c_void_p, ctypes.c_double,
                             ctypes.c_double, ctypes.POINTER(ctypes.c_double)]
lib.es_destroy.restype  = None
lib.es_destroy.argtypes = [ctypes.c_void_p]

svc = lib.es_create(b"/data/dted_maps/")
msl = ctypes.c_double()
rc  = lib.es_get_elev(svc, 32.0853, 34.7818, ctypes.byref(msl))
if rc == 0:
    print(f"{msl.value:.1f} m")
lib.es_destroy(svc)
```

### C# (P/Invoke)

```csharp
using System.Runtime.InteropServices;

static class Elev {
    [DllImport("elevation_dll.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr es_create([MarshalAs(UnmanagedType.LPStr)] string tilesDir);

    [DllImport("elevation_dll.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern int es_get_elev(IntPtr svc, double lat, double lon, out double outMsl);

    [DllImport("elevation_dll.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern void es_destroy(IntPtr svc);
}

var svc = Elev.es_create(@"C:\data\dted_maps\");
int rc  = Elev.es_get_elev(svc, 32.0853, 34.7818, out double msl);
if (rc == 0) Console.WriteLine($"{msl:F1} m");
Elev.es_destroy(svc);
```

---

## POC consumer (`poc_consumer.exe`)

A ready-made interactive test tool is included at `test/poc_consumer.cpp`.

```
poc_consumer.exe [--tiles <dir>] [lat lon ...]
```

| Usage | Behaviour |
|-------|-----------|
| `poc_consumer.exe --tiles C:\data\dted_maps` | Runs preset locations then enters interactive mode |
| `poc_consumer.exe --tiles C:\data\dted_maps 32.08 34.78` | Queries one point and exits |
| `poc_consumer.exe` | Uses default path `<exe_dir>\..\..\..\dted_maps` |

In interactive mode type `lat lon` and press Enter, or `q` to quit.

---

## Error codes

| Code | Value | Meaning |
|------|-------|---------|
| `ES_OK` | 0 | Success; `*out_msl` is valid |
| `ES_ERR_BAD_ARG` | -1 | Null pointer, or lat/lon outside `[-90,90]`/`[-180,180]` |
| `ES_ERR_NO_DATA` | -2 | No tile on disk for this coordinate, or DTED void post |
| `ES_ERR_IO` | -3 | File found but wrong size or mmap failed |
