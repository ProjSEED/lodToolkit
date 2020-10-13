file(GLOB OSGBTO3MX_HEADER *.h)
file(GLOB OSGBTO3MX_SRC *.cpp)

add_executable(osgbTo3mx ${OSGBTO3MX_SRC} ${OSGBTO3MX_HEADER})
target_link_libraries(osgbTo3mx core ${THIRDPARTY_LIBS})
set_property(TARGET osgbTo3mx PROPERTY FOLDER "app")  

