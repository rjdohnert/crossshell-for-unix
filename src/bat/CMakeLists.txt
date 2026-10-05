cmake_minimum_required(VERSION 3.15)
project(bat VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(BAT_SOURCES
    bat.cpp
    bat_app.cpp
    engine.cpp
    highlighter.cpp
    options.cpp
    reporter.cpp
)

set(BAT_HEADERS
    bat.hpp
    bat_app.hpp
    color.hpp
    engine.hpp
    highlighter.hpp
    options.hpp
    reporter.hpp
)

add_executable(bat ${BAT_SOURCES} ${BAT_HEADERS})
target_include_directories(bat PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
