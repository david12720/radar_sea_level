# Build Reference

## Windows — Visual Studio 2022

The project is already added to `server/radar_sea_level.sln`.

**DLL project** (`elevation_dll.vcxproj`):
- Output: `$(Platform)\$(Configuration)\elevation_dll.dll` + `.lib`
- Configurations: Debug | x64, Release | x64, Debug | Win32, Release | Win32
- Preprocessor: `ELEVATION_DLL_EXPORTS` (switches header to `dllexport`)
- Include dirs: `include\` (project-local only)
- C++ standard: C++17

**Consumer / test project** (`test\poc_consumer.vcxproj`):
- Output: `$(Platform)\$(Configuration)\poc_consumer.exe`
- Links against: `$(Platform)\$(Configuration)\elevation_dll.lib`
- At runtime: `elevation_dll.dll` must be in the same directory as the exe
  (copy manually after build, or set the VS debugger working directory)

The existing projects (`radar_server`, `lut_tcp_server`, etc.) are unchanged —
`elevation_dll` is an independent project, not a dependency of anything else.

### Building with an older toolset (v141 / VS2017)

The vcxproj defaults to `v143` (VS2022). To build with a different toolset
without editing the file, pass it on the MSBuild command line:

```powershell
& "C:\...\MSBuild.exe" elevation_dll.vcxproj `
  /p:Configuration=Release /p:Platform=x64 `
  /p:PlatformToolset=v141 `
  /p:WindowsTargetPlatformVersion=10.0.26100.0
```

The toolset must be installed via the Visual Studio Installer
(**Individual components → MSVC v141 – VS 2017 C++ x64/x86 build tools**).

### Testing the C++98 mutex fallback on a modern compiler

Define `ELEV_NO_STDMUTEX` to force the `CRITICAL_SECTION` path regardless of
the C++ standard in use. Useful to verify the fallback compiles and runs
correctly without installing an old compiler:

```powershell
& "C:\...\MSBuild.exe" elevation_dll.vcxproj /p:Configuration=Release /p:Platform=x64 `
  "/p:PreprocessorDefinitions=ELEVATION_DLL_EXPORTS;ELEV_NO_STDMUTEX;_WIN32_WINNT=0x0A00;NDEBUG"
```

Remove the define to restore normal C++11 `std::mutex` behaviour.

### Adding to a different solution

1. Add `elevation_dll.vcxproj` to the solution.
2. Set `ELEVATION_DLL_EXPORTS` in the DLL project preprocessor.
3. In the consuming project: add `include\` to additional include directories
   and add the `.lib` to additional link dependencies.

---

## Linux — Makefile

```make
CXX      ?= g++
CXXFLAGS ?= -O2 -Wall -Wextra -fPIC -std=c++11 -pthread
LDFLAGS  ?= -shared -pthread
TARGET   := libelevation.so
SRCS     := src/elevation_service.cpp src/tile_store.cpp \
            src/bicubic.cpp src/mapped_file.cpp
OBJS     := $(SRCS:.cpp=.o)

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -DELEVATION_DLL_EXPORTS -Iinclude -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
```

Build:

```bash
cd server/elevation_dll
make
```

Build with strict C++98 (tests the `pthread_mutex_t` fallback path):

```bash
make CXX=g++ CXXFLAGS="-O2 -Wall -Wextra -fPIC -std=c++98 -pthread"
```

Build with MinGW on Windows (MSYS2):

```bash
# In an MSYS2 MinGW64 shell:
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-make
cd /c/Users/.../radar_sea_level/server/elevation_dll
mingw32-make CXX=g++ CXXFLAGS="-O2 -Wall -Wextra -fPIC -std=c++98 -pthread"
```

Link a consumer:

```bash
g++ -std=c++11 -Iinclude consumer.cpp -L. -lelevation -Wl,-rpath,. -o consumer
```
