cmake_minimum_required(VERSION 3.15)
project(chattr VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CHATTR_SOURCES
    chattr.cpp
    chattr_app.cpp
    engine.cpp
    options.cpp
    reporter.cpp
)

set(CHATTR_HEADERS
    chattr.hpp
    chattr_app.hpp
    engine.hpp
    models.hpp
    options.hpp
    reporter.hpp
)

add_executable(chattr ${CHATTR_SOURCES} ${CHATTR_HEADERS})
target_include_directories(chattr PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
