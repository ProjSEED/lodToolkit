cmake_minimum_required(VERSION 2.8)

Project(PlyIO)

include_directories(./include)

set(CMAKE_DEBUG_POSTFIX d)

file(GLOB HEADER include/*.h)
file(GLOB SRC src/*.cpp)

add_library(PlyIO STATIC ${SRC} ${HEADER})