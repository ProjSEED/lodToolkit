include_directories(./include)

file(GLOB CORE_HEADER ./include/*.h)
file(GLOB CORE_SRC ./src/*.cpp)

# image
file (GLOB IMAGE_SRC ./image/*.cpp)
file (GLOB IMAGE_H ./image/*.h)
source_group("image" FILES ${IMAGE_SRC} ${IMAGE_H} )
include_directories("./image/")

add_definitions(-DOPENCTM_STATIC)

add_library(core STATIC ${CORE_SRC} ${CORE_HEADER} ${IMAGE_SRC} ${IMAGE_H})
set_property(TARGET core PROPERTY FOLDER "lib")  