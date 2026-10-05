cmake_minimum_required(VERSION 3.15)
project(bdf VERSION 5.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(BDF_SOURCES
    bdf.cpp
    bdf_app.cpp
    formatter.cpp
    inspector.cpp
    options.cpp
    pipeline.cpp
    table.cpp
)

set(BDF_HEADERS
    bdf.hpp
    bdf_app.hpp
    formatter.hpp
    inspector.hpp
    options.hpp
    pipeline.hpp
    table.hpp
)

add_executable(bdf ${BDF_SOURCES} ${BDF_HEADERS})
target_include_directories(bdf PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
