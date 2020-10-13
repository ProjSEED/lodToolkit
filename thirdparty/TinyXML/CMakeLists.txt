file(GLOB TINYXML_HEADER src/*.h)
file(GLOB TINYXML_SRC src/*.cpp)

add_library(TinyXML STATIC ${TINYXML_HEADER} ${TINYXML_SRC})
set_property(TARGET TinyXML PROPERTY FOLDER "thirdparty")  