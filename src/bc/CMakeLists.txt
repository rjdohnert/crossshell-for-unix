cmake_minimum_required(VERSION 3.15)
project(bc VERSION 7.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(BC_SOURCES
    bc.cpp
    bc_app.cpp
    engine.cpp
    options.cpp
    parser.cpp
    reporter.cpp
)

set(BC_HEADERS
    bc.hpp
    bc_app.hpp
    engine.hpp
    options.hpp
    parser.hpp
    reporter.hpp
)

add_executable(bc ${BC_SOURCES} ${BC_HEADERS})
target_include_directories(bc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
