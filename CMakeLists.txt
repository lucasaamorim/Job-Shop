cmake_minimum_required(VERSION 3.10)

project(Job-Shop VERSION 0.1 LANGUAGES CXX CC)

# Temporário, ainda tem muita coisa pra fazer
set(CMAKE_CXX_STANDARD 20)
set( CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set( CMAKE_CXX_FLAGS_RELEASE "-Ofast")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib)

add_executable(Job-Shop main.cpp)
