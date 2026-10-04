cmake_minimum_required(VERSION 3.15)
project(arch VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ARCH_SOURCES
    arch.cpp
    arch_app.cpp
    engine.cpp
    options.cpp
    reporter.cpp
)

set(ARCH_HEADERS
    arch.hpp
    arch_app.hpp
    engine.hpp
    options.hpp
    reporter.hpp
)

add_executable(arch ${ARCH_SOURCES} ${ARCH_HEADERS})
target_include_directories(arch PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
