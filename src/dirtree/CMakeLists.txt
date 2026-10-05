cmake_minimum_required(VERSION 3.15)
project(dirtree LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    dirtree.cpp
    dirtree_app.cpp
    traversal_engine.cpp
    path_helper.cpp
    options.cpp
)

set(HEADERS
    dirtree.hpp
    dirtree_app.hpp
    traversal_engine.hpp
    path_helper.hpp
    options.hpp
    models.hpp
)

add_executable(dirtree ${SOURCES} ${HEADERS})

set_target_properties(dirtree PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../../bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/../../bin"
)
