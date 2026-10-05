cmake_minimum_required(VERSION 3.15)
project(clock VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CLOCK_SOURCES
    clock.cpp
    clock_app.cpp
    engine.cpp
    options.cpp
    reporter.cpp
)

set(CLOCK_HEADERS
    clock.hpp
    clock_app.hpp
    models.hpp
    engine.hpp
    options.hpp
    reporter.hpp
)

add_executable(clock ${CLOCK_SOURCES} ${CLOCK_HEADERS})
target_include_directories(clock PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
