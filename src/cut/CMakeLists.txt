cmake_minimum_required(VERSION 3.15)
project(cut VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CUT_SOURCES
    cut.cpp
    cut_app.cpp
    range_parser.cpp
    stream_processor.cpp
    options.cpp
)

set(CUT_HEADERS
    cut.hpp
    cut_app.hpp
    models.hpp
    range_parser.hpp
    stream_processor.hpp
    options.hpp
)

add_executable(cut ${CUT_SOURCES} ${CUT_HEADERS})
target_include_directories(cut PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
