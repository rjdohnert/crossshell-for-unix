cmake_minimum_required(VERSION 3.15)
project(diff LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    diff.cpp
    diff_app.cpp
    arg_parser.cpp
    formatter.cpp
    myers_diff.cpp
    line_reader.cpp
    normalizer.cpp
)

set(HEADERS
    diff.hpp
    diff_app.hpp
    arg_parser.hpp
    formatter.hpp
    myers_diff.hpp
    line_reader.hpp
    normalizer.hpp
    models.hpp
)

add_executable(diff ${SOURCES} ${HEADERS})

set_target_properties(diff PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../../bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/../../bin"
)
