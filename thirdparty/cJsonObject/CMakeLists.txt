include_directories(./include)

set(CJSONOBJECT_SRC 
	./src/CJsonObject.cpp
	./src/cJSON.c
)

set(CJSONOBJECT_H
	./include/CJsonObject.hpp
    ./include/cJSON.h
)

add_library(cJsonObject STATIC ${CJSONOBJECT_SRC} ${CJSONOBJECT_H})
set_property(TARGET cJsonObject PROPERTY FOLDER "thirdparty")  