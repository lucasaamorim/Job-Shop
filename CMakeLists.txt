cmake_minimum_required(VERSION 3.10)

project(Job-Shop VERSION 0.2 LANGUAGES CXX)

# Project settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-Ofast")

# --- Main Executable ---
add_executable(Job-Shop
    src/main.cpp
    src/lib/ScheduleState.cpp
    src/lib/Schedule.cpp
    src/lib/solvers/BBSolver.cpp
    src/lib/solvers/DispatchSolver.cpp
)

target_include_directories(Job-Shop PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/lib)
# --- Test Utilities ---

# Compile testgen.c
# add_executable(testgen tests/testgen.c)

# Sample test
add_custom_target(run_test
    COMMAND $<TARGET_FILE:Job-Shop> -r shortest_processing_time ${CMAKE_CURRENT_SOURCE_DIR}/tests/instance_sets/ft/ft06.txt
    DEPENDS Job-Shop
    COMMENT "Running test with ft06 instance using shortest_processing_time rule..."
)
