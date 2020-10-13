add_definitions(-DLASZIPDLL_EXPORTS)
file(GLOB LASZIP_HEADER src/*.hpp)
file(GLOB LASZIP_SRC src/*.cpp)

add_library(LASzip SHARED ${LASZIP_HEADER} ${LASZIP_SRC})
set_property(TARGET LASzip PROPERTY FOLDER "thirdparty")  