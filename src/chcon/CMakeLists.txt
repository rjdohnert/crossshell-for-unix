cmake_minimum_required(VERSION 3.15)
project(chcon VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CHCON_SOURCES
    chcon.cpp
    chcon_app.cpp
    security.cpp
    options.cpp
    reporter.cpp
)

set(CHCON_HEADERS
    chcon.hpp
    chcon_app.hpp
    models.hpp
    security.hpp
    options.hpp
    reporter.hpp
)

add_executable(chcon ${CHCON_SOURCES} ${CHCON_HEADERS})
target_include_directories(chcon PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if (WIN32)
    target_link_libraries(chcon PRIVATE advapi32)
endif()
