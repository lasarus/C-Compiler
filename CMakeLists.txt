cmake_minimum_required(VERSION 3.10)
project(cc)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED True)

include_directories(src)

file(GLOB SOURCES "src/*.c" "src/*/*.c")

add_executable(cc ${SOURCES})
