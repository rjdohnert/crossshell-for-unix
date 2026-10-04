cmake_minimum_required(VERSION 3.15)
project(autotool VERSION 4.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(AUTOTOOL_SOURCES
    autoconf.cpp
    automake.cpp
    autotool.cpp
    autotool_app.cpp
    build_engine.cpp
    help.cpp
    libtool.cpp
    port_engine.cpp
    utils.cpp
)

set(AUTOTOOL_HEADERS
    autoconf.hpp
    automake.hpp
    autotool.hpp
    autotool_app.hpp
    build_engine.hpp
    help.hpp
    libtool.hpp
    port_engine.hpp
    types.hpp
    utils.hpp
)

add_executable(autotool ${AUTOTOOL_SOURCES} ${AUTOTOOL_HEADERS})
target_include_directories(autotool PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
