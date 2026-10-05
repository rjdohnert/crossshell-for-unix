cmake_minimum_required(VERSION 3.15)
project(chgrp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CHGRP_SOURCES
    chgrp.cpp
    chgrp_app.cpp
    group_manager.cpp
    options.cpp
    reporter.cpp
)

set(CHGRP_HEADERS
    chgrp.hpp
    chgrp_app.hpp
    models.hpp
    group_manager.hpp
    options.hpp
    reporter.hpp
)

add_executable(chgrp ${CHGRP_SOURCES} ${CHGRP_HEADERS})
target_include_directories(chgrp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if (WIN32)
    target_link_libraries(chgrp PRIVATE advapi32)
endif()
