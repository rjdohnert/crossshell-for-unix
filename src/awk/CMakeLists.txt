cmake_minimum_required(VERSION 3.15)
project(awk VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(AWK_SOURCES
    awk.cpp
    awk_app.cpp
    context.cpp
    evaluator.cpp
    parser.cpp
    string_utils.cpp
    value.cpp
    win_loader.cpp
)

set(AWK_HEADERS
    awk.hpp
    awk_app.hpp
    context.hpp
    evaluator.hpp
    parser.hpp
    string_utils.hpp
    value.hpp
    win_loader.hpp
)

add_executable(awk ${AWK_SOURCES} ${AWK_HEADERS})
target_include_directories(awk PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(WIN32)
    target_link_libraries(awk PRIVATE wbemuuid ole32 oleaut32 advapi32)
endif()
