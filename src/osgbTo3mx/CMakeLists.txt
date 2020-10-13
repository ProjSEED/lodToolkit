file(GLOB OSGBTO3MX_HEADER *.h)
file(GLOB OSGBTO3MX_SRC *.cpp)

# image
file (GLOB IMAGE_SRC ./image/*.cpp)
file (GLOB IMAGE_H ./image/*.h)
source_group("image" FILES ${IMAGE_SRC} ${IMAGE_H} )
include_directories("./image/")

add_definitions(-DOPENCTM_STATIC)

add_executable(osgbTo3mx ${OSGBTO3MX_SRC} ${OSGBTO3MX_HEADER} ${IMAGE_SRC} ${IMAGE_H})
target_link_libraries(osgbTo3mx core ${THIRDPARTY_LIBS})
set_property(TARGET osgbTo3mx PROPERTY FOLDER "app")  

