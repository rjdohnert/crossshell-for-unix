cmake_minimum_required(VERSION 3.15)
project(chroot VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CHROOT_SOURCES
    chroot.cpp
    chroot_app.cpp
    path_helper.cpp
    options.cpp
    executor.cpp
)

set(CHROOT_HEADERS
    chroot.hpp
    chroot_app.hpp
    models.hpp
    path_helper.hpp
    options.hpp
    executor.hpp
)

add_executable(chroot ${CHROOT_SOURCES} ${CHROOT_HEADERS})
target_include_directories(chroot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
