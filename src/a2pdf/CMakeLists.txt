cmake_minimum_required(VERSION 3.15)
project(a2pdf VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(A2PDF_SOURCES
    a2pdf.cpp
    a2pdf_app.cpp
    cli.cpp
    encoding.cpp
    output_reporter.cpp
    pdf_generator.cpp
    ps_generator.cpp
    text_processor.cpp
)

set(A2PDF_HEADERS
    a2pdf.hpp
    a2pdf_app.hpp
    cli.hpp
    config.hpp
    encoding.hpp
    output_reporter.hpp
    pdf_generator.hpp
    ps_generator.hpp
    text_processor.hpp
)

add_executable(a2pdf ${A2PDF_SOURCES} ${A2PDF_HEADERS})
target_include_directories(a2pdf PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
