cmake_minimum_required(VERSION 3.15)
project(chmod VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CHMOD_SOURCES
    chmod.cpp
    chmod_app.cpp
    acl_manager.cpp
    mode_parser.cpp
    options.cpp
    reporter.cpp
)

set(CHMOD_HEADERS
    chmod.hpp
    chmod_app.hpp
    models.hpp
    acl_manager.hpp
    mode_parser.hpp
    options.hpp
    reporter.hpp
)

add_executable(chmod ${CHMOD_SOURCES} ${CHMOD_HEADERS})
target_include_directories(chmod PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if (WIN32)
    target_link_libraries(chmod PRIVATE advapi32)
endif()
