file(GLOB HEADER *.h)
file(GLOB SRC *.cpp)

add_executable(${CMAKE_PROJECT_NAME} ${SRC} ${HEADER})
target_link_libraries(${CMAKE_PROJECT_NAME} ${THIRDPARTY_LIBS})