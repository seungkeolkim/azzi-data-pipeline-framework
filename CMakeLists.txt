cmake_minimum_required(VERSION 3.20)
project(stream_pipeline LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(stream_pipeline_headers INTERFACE)
target_include_directories(stream_pipeline_headers INTERFACE include)
