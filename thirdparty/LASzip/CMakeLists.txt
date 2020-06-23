cmake_minimum_required(VERSION 2.8)

Project(LASzip)

include_directories(./dll_build)

set(CMAKE_DEBUG_POSTFIX d)

add_definitions(-DLASZIPDLL_EXPORTS)
file(GLOB HEADER src/*.hpp)
file(GLOB SRC src/*.cpp)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../lib)

add_library(LASzip SHARED ${SRC} ${HEADER})
add_library(LASzip_s STATIC ${SRC} ${HEADER})