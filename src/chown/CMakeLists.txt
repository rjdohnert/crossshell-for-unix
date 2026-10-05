cmake_minimum_required(VERSION 3.15)
project(chown VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CHOWN_SOURCES
    chown.cpp
    chown_app.cpp
    ownership.cpp
    options.cpp
    reporter.cpp
)

set(CHOWN_HEADERS
    chown.hpp
    chown_app.hpp
    models.hpp
    ownership.hpp
    options.hpp
    reporter.hpp
)

add_executable(chown ${CHOWN_SOURCES} ${CHOWN_HEADERS})
target_include_directories(chown PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if (WIN32)
    target_link_libraries(chown PRIVATE advapi32)
endif()
