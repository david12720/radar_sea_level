cmake_minimum_required(VERSION 3.15)
project(radar_sea_level CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Compiler flags ────────────────────────────────────────────────────────────
if(MSVC)
    add_compile_options(/W3 /O2)
else()
    add_compile_options(-Wall -Wextra -O2)
endif()

# ── Windows: Winsock (required by cpp-httplib) ────────────────────────────────
if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0A00)
    set(PLATFORM_LIBS ws2_32)
else()
    set(PLATFORM_LIBS pthread)
endif()

# ── Shared source files ───────────────────────────────────────────────────────
set(CORE_SOURCES
    src/dted_tile.cpp
    src/dem_database.cpp
    src/elevation_lut.cpp
    src/radar_compute.cpp
    src/relative_angle.cpp
)

# ── Demo binary ───────────────────────────────────────────────────────────────
add_executable(radar_sea_level
    main.cpp
    ${CORE_SOURCES}
)
target_include_directories(radar_sea_level PRIVATE include)

# ── HTTP server binary ────────────────────────────────────────────────────────
add_executable(radar_server
    main_server.cpp
    app/query_handler.cpp
    app/radar_server.cpp
    ${CORE_SOURCES}
)
target_include_directories(radar_server PRIVATE include app)
target_link_libraries(radar_server PRIVATE ${PLATFORM_LIBS})

# ── Unit tests ────────────────────────────────────────────────────────────────
add_executable(test_query_handler
    tests/test_query_handler.cpp
    app/query_handler.cpp
    ${CORE_SOURCES}
)
target_include_directories(test_query_handler PRIVATE include app)
target_link_libraries(test_query_handler PRIVATE ${PLATFORM_LIBS})
