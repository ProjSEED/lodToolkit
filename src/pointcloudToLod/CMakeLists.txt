file(GLOB POINTCLOUDTOLOD_HEADER *.h)
file(GLOB POINTCLOUDTOLOD_SRC *.cpp)

add_executable(pointcloudToLod ${POINTCLOUDTOLOD_SRC} ${POINTCLOUDTOLOD_HEADER})
target_link_libraries(pointcloudToLod core ${THIRDPARTY_LIBS})
set_property(TARGET pointcloudToLod PROPERTY FOLDER "app")  