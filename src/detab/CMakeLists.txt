cmake_minimum_required(VERSION 3.15)
project(detab LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    detab.cpp
    detab_app.cpp
    engine.cpp
    reporter.cpp
    options.cpp
)

set(HEADERS
    detab.hpp
    detab_app.hpp
    engine.hpp
    reporter.hpp
    options.hpp
    models.hpp
)

add_executable(detab ${SOURCES} ${HEADERS})

set_target_properties(detab PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../../bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/../../bin"
)
