cmake_minimum_required(VERSION 3.20)
project(csot_quant_platform CXX)

set(CMAKE_CXX_STANDARD          20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)
add_compile_options($<$<CONFIG:Release>:-O2>)
add_compile_options($<$<CONFIG:Release>:-fno-omit-frame-pointer>)

include_directories(${CMAKE_SOURCE_DIR}/include)

add_library(engine STATIC src/engine.cpp)
target_link_libraries(engine PUBLIC ${CMAKE_DL_LIBS})

add_executable(quant_runner src/main.cpp)
target_link_libraries(quant_runner PRIVATE engine)

add_library(spec_strategy SHARED strategies/spec_strategy.cpp)
set_target_properties(spec_strategy PROPERTIES PREFIX "")

find_package(benchmark QUIET)
if(benchmark_FOUND)
    add_executable(quant_bench bench/bench.cpp)
    target_link_libraries(quant_bench PRIVATE engine benchmark::benchmark)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")