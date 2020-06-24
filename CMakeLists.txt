cmake_minimum_required(VERSION 2.8)

Project(ToOSGB)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include_directories(./thirdparty/LASzip/include)
include_directories(./thirdparty/TinyXml/include)
include_directories(./thirdparty/PlyIO/include)

find_package(OpenSceneGraph REQUIRED osgDB osgUtil) # libOpenThreads & libosg automatically searched
include_directories(${OPENSCENEGRAPH_INCLUDE_DIRS})

add_subdirectory(thirdparty)

set(THIRDPARTY_LIBS
	LASzip
	PlyIO
	TinyXML
	${OPENSCENEGRAPH_LIBRARIES}
)
add_subdirectory(src)
