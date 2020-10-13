file(GLOB MESHTOLOD_HEADER *.h)
file(GLOB MESHTOLOD_SRC *.cpp)

add_executable(meshToLod ${MESHTOLOD_SRC} ${MESHTOLOD_HEADER})
target_link_libraries(meshToLod core ${THIRDPARTY_LIBS})
set_property(TARGET meshToLod PROPERTY FOLDER "app")  