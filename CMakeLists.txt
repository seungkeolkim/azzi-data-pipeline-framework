cmake_minimum_required(VERSION 3.14)
project(azzi_data_pipeline_framework LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

file(GLOB_RECURSE AZZI_SOURCES
    src/*.cpp
)

add_executable(azzi_pipeline ${AZZI_SOURCES})

target_compile_options(azzi_pipeline PRIVATE -Wall -Wextra -pedantic)
