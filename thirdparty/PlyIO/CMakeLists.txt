include_directories(./include)

file(GLOB PLY_IO_HEADER include/*.h)
file(GLOB PLY_IO_SRC src/*.cpp)

add_library(PlyIO STATIC ${PLY_IO_SRC} ${PLY_IO_HEADER})