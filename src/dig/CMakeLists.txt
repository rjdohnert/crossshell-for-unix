cmake_minimum_required(VERSION 3.15)
project(dig LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    dig.cpp
    dig_app.cpp
    options.cpp
    query_engine.cpp
    reporter.cpp
)

set(HEADERS
    dig.hpp
    dig_app.hpp
    options.hpp
    query_engine.hpp
    reporter.hpp
    models.hpp
)

add_executable(dig ${SOURCES} ${HEADERS})

if (WIN32)
    target_link_libraries(dig PRIVATE dnsapi ws2_32)
endif()

set_target_properties(dig PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../../bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/../../bin"
)
