cmake_minimum_required(VERSION 3.15)
project(diff3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    diff3.cpp
    diff3_app.cpp
    arg_parser.cpp
    formatter.cpp
    diff3_engine.cpp
    myers_diff.cpp
    line_reader.cpp
)

set(HEADERS
    diff3.hpp
    diff3_app.hpp
    arg_parser.hpp
    formatter.hpp
    diff3_engine.hpp
    myers_diff.hpp
    line_reader.hpp
    models.hpp
)

add_executable(diff3 ${SOURCES} ${HEADERS})

set_target_properties(diff3 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../../bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/../../bin"
)
