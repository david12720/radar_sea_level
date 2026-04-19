cmake_minimum_required(VERSION 3.15)
project(radar_sea_level CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(radar_sea_level
    main.cpp
    src/dted_tile.cpp
    src/dem_database.cpp
    src/elevation_lut.cpp
    src/radar_compute.cpp
)

target_include_directories(radar_sea_level PRIVATE include)
target_compile_options(radar_sea_level PRIVATE -Wall -Wextra -O2)
