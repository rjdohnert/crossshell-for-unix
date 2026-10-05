cmake_minimum_required(VERSION 3.15)
project(dircolors LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    dircolors.cpp
    dircolors_app.cpp
    engine.cpp
    reporter.cpp
    database_parser.cpp
    options.cpp
)

set(HEADERS
    dircolors.hpp
    dircolors_app.hpp
    engine.hpp
    reporter.hpp
    database_parser.hpp
    options.hpp
    models.hpp
)

add_executable(dircolors ${SOURCES} ${HEADERS})

set_target_properties(dircolors PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../../bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/../../bin"
)
