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

Link a consumer:

```bash
g++ -std=c++11 -Iinclude consumer.cpp -L. -lelevation -Wl,-rpath,. -o consumer
```
