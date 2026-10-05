cmake_minimum_required(VERSION 3.15)
project(dd VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(DD_SOURCES
    dd.cpp
    dd_app.cpp
    block_copier.cpp
    data_transformer.cpp
    size_parser.cpp
    options.cpp
)

set(DD_HEADERS
    dd.hpp
    dd_app.hpp
    models.hpp
    block_copier.hpp
    data_transformer.hpp
    size_parser.hpp
    options.hpp
)

add_executable(dd ${DD_SOURCES} ${DD_HEADERS})
target_include_directories(dd PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
