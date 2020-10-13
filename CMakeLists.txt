cmake_minimum_required(VERSION 2.8)

Project(lodToolkit)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)  

# c++ 17
if (MSVC_VERSION GREATER_EQUAL "1900")
    include(CheckCXXCompilerFlag)
    CHECK_CXX_COMPILER_FLAG("/std:c++latest" _cpp_latest_flag_supported)
    if (_cpp_latest_flag_supported)
        add_compile_options("/std:c++latest")
    endif()
endif()

find_package(OpenSceneGraph REQUIRED osgDB osgUtil) # libOpenThreads & libosg automatically searched
include_directories(${OPENSCENEGRAPH_INCLUDE_DIRS})

include_directories(./thirdparty/CmdParser/include)
include_directories(./thirdparty/LASzip/include)
include_directories(./thirdparty/PlyIO/include)
include_directories(./thirdparty/cJsonObject/include)
include_directories(./thirdparty/openCTM/include)

add_subdirectory(thirdparty)

set(THIRDPARTY_LIBS
	LASzip
	PlyIO
	cJsonObject
	openCTM
	${OPENSCENEGRAPH_LIBRARIES}
)

include_directories(./src/core/include)
add_subdirectory(src)
