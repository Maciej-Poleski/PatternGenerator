cmake_minimum_required(VERSION 3.10)

project(PatternGenerator)

set(CMAKE_CXX_STANDARD 17)

add_library(ppm ppm.cxx matrix.cxx)

add_executable(generate_patterns generate_patterns.cxx ppm)
