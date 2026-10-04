cmake_minimum_required(VERSION 3.15)
project(auditctl VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(AUDITCTL_SOURCES
    auditctl.cpp
    auditctl_app.cpp
    audit_policy.cpp
    audit_session.cpp
    audit_types.cpp
    cli.cpp
)

set(AUDITCTL_HEADERS
    auditctl.hpp
    auditctl_app.hpp
    audit_policy.hpp
    audit_session.hpp
    audit_types.hpp
    cli.hpp
)

add_executable(auditctl ${AUDITCTL_SOURCES} ${AUDITCTL_HEADERS})
target_link_libraries(auditctl PRIVATE advapi32)
target_include_directories(auditctl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
