cmake_minimum_required(VERSION 3.15)
project(dc VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(DC_SOURCES
    dc.cpp
    dc_app.cpp
    calculator.cpp
    values.cpp
    big_number.cpp
)

set(DC_HEADERS
    dc.hpp
    dc_app.hpp
    calculator.hpp
    values.hpp
    big_number.hpp
)

add_executable(dc ${DC_SOURCES} ${DC_HEADERS})
target_include_directories(dc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
