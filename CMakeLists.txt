cmake_minimum_required(VERSION 3.16)
project(azzi_data_pipeline_framework LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(azzi_pipeline
    src/main.cpp
    src/utils/logger.h
    src/utils/bounded_pointer_queue.h
    src/metadata/frame.h
    src/metadata/channel_runtime_meta.h
    src/store/channel_runtime_meta_store.h
    src/store/node_runtime_state_store.h
    src/pipeline/nodes/dummy_decode_node.h
    src/pipeline/nodes/dummy_detection_node.h
    src/pipeline/nodes/output_node.h
)

target_include_directories(azzi_pipeline PRIVATE src)
