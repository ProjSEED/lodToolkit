include_directories(./include)

# liblzma
file (GLOB LIBLZMA_SRC ./liblzma/*.c)
file (GLOB LIBLZMA_H ./liblzma/*.h)
source_group("liblzma" FILES ${LIBLZMA_SRC} ${LIBLZMA_H} )
include_directories("./liblzma/")

# openctm
file (GLOB OPENCTM_SRC ./src/*.c)
file (GLOB OPENCTM_H ./include/*.h)
source_group("openCTM" FILES ${OPENCTM_SRC} ${OPENCTM_H} )

add_definitions(-DOPENCTM_STATIC)

add_library(openCTM STATIC ${OPENCTM_SRC} ${OPENCTM_H} ${LIBLZMA_SRC} ${LIBLZMA_H})
set_property(TARGET openCTM PROPERTY FOLDER "thirdparty")  