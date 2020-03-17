cmake_minimum_required(VERSION 2.8)

Project(Laz2Osgb)

include_directories(./thirdparty/liblas/include)
include_directories(./thirdparty/TinyXml/include)
include_directories(./thirdparty/OSG/include)

find_package(Boost)
include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})

file(GLOB HEADER src/*.h)
file(GLOB SRC src/*.cpp)

add_executable(las2osgb ${SRC} ${HEADER})
target_link_libraries(las2osgb ${Boost_LIBRARIES})