cmake_minimum_required(VERSION 2.8)

Project(OsgbCreator)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

include_directories(./thirdparty/liblas/include)
#include_directories(./thirdparty/LASlib/inc)
include_directories(./thirdparty/LASzip/include)
include_directories(./thirdparty/TinyXml/include)
include_directories(./thirdparty/OSG/include)
include_directories(./thirdparty/PlyIO/include)

find_package(Boost)
include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})

file(GLOB HEADER src/*.h)
file(GLOB SRC src/*.cpp)

add_executable(OsgbCreator ${SRC} ${HEADER})
target_link_libraries(las2osgb ${Boost_LIBRARIES})