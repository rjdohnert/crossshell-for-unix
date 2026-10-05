cmake_minimum_required(VERSION 3.15)
project(base VERSION 8.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(BASE_SOURCES
    base.cpp
    base_app.cpp
    codec.cpp
    engine.cpp
    options.cpp
    reporter.cpp
)

set(BASE_HEADERS
    base.hpp
    base_app.hpp
    codec.hpp
    engine.hpp
    options.hpp
    reporter.hpp
)

add_executable(base ${BASE_SOURCES} ${BASE_HEADERS})
target_include_directories(base PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
