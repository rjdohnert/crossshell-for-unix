cmake_minimum_required(VERSION 3.15)
project(dmesg LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    dmesg.cpp
    dmesg_app.cpp
    engine.cpp
    event_reader.cpp
    xml_helper.cpp
    options.cpp
)

set(HEADERS
    dmesg.hpp
    dmesg_app.hpp
    engine.hpp
    event_reader.hpp
    xml_helper.hpp
    options.hpp
    models.hpp
)

add_executable(dmesg ${SOURCES} ${HEADERS})

if (WIN32)
    target_link_libraries(dmesg PRIVATE wevtapi)
endif()

set_target_properties(dmesg PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../../bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/../../bin"
)
