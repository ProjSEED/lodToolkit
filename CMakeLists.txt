cmake_minimum_required(VERSION 2.8)

Project(ToOSGB)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include_directories(./thirdparty/LASzip/include)
include_directories(./thirdparty/TinyXml/include)
include_directories(./thirdparty/OSG/include)
include_directories(./thirdparty/PlyIO/include)

add_subdirectory(thirdparty)
link_directories(${CMAKE_SOURCE_DIR}/thirdparty/OSG/lib/)
set(OSG_LIBS
	osg
	osgDB
	osgUtil
	OpenThreads
)
set(THIRDPARTY_LIBS
	LASzip
	PlyIO
	TinyXML
	${OSG_LIBS}
)
add_subdirectory(src)

#target_link_libraries(OsgbCreator ${Boost_LIBRARIES})