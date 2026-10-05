cmake_minimum_required(VERSION 3.15)
project(basename VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(BASENAME_SOURCES
    basename.cpp
    basename_app.cpp
    engine.cpp
    options.cpp
    reporter.cpp
)

set(BASENAME_HEADERS
    basename.hpp
    basename_app.hpp
    engine.hpp
    options.hpp
    reporter.hpp
)

add_executable(basename ${BASENAME_SOURCES} ${BASENAME_HEADERS})
target_include_directories(basename PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
