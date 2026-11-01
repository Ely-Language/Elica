cmake_minimum_required(VERSION 3.50)
project(elica_compiler CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

include_directories(include external)

file(GLOB_RECURSE SOURCES "src/*.cpp")


add_executable(elica src/main.cpp ${SOURCES})
