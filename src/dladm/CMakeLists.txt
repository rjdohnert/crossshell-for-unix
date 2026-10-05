cmake_minimum_required(VERSION 3.15)
project(dladm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    dladm.cpp
    dladm_app.cpp
    commands.cpp
    link_manager.cpp
    models.cpp
)

set(HEADERS
    dladm.hpp
    dladm_app.hpp
    commands.hpp
    link_manager.hpp
    models.hpp
)

add_executable(dladm ${SOURCES} ${HEADERS})

if (WIN32)
    target_link_libraries(dladm PRIVATE iphlpapi ws2_32)
endif()

set_target_properties(dladm PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../../bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/../../bin"
)
