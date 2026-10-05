cmake_minimum_required(VERSION 3.15)
project(dirname LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    dirname.cpp
    dirname_app.cpp
    engine.cpp
    reporter.cpp
    options.cpp
)

set(HEADERS
    dirname.hpp
    dirname_app.hpp
    engine.hpp
    reporter.hpp
    options.hpp
)

add_executable(dirname ${SOURCES} ${HEADERS})

set_target_properties(dirname PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../../bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/../../bin"
)
