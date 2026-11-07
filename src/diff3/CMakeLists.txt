cmake_minimum_required(VERSION 3.15)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/diff3.c")
    project(diff3 LANGUAGES C CXX)
    set(SOURCES diff3.c)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/diff3_app.cpp")
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
else()
    project(diff3 LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(SOURCES diff3.cpp)
endif()

add_executable(diff3 ${SOURCES})

set_target_properties(diff3 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../../bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/../../bin"
)
