include_directories(./include)

file(GLOB CORE_HEADER ./include/*.h)
file(GLOB CORE_SRC ./src/*.cpp)

add_library(core STATIC ${CORE_SRC} ${CORE_HEADER})
set_property(TARGET core PROPERTY FOLDER "lib")  