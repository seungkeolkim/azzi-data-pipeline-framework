cmake_minimum_required(VERSION 3.16)

project(stream_pipeline_framework
    VERSION 0.1.0
    LANGUAGES CXX
)

# --- Build settings ----------------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# clangd / IDE tooling
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Reasonable defaults
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

# --- Sources ----------------------------------------------------------------
# Stage 0: build one executable
# - Note: Using GLOB with CONFIGURE_DEPENDS keeps the list updated when files are added/removed.
file(GLOB_RECURSE STREAM_PIPELINE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_executable(stream_pipeline_stage0
    ${STREAM_PIPELINE_SOURCES}
)

target_include_directories(stream_pipeline_stage0
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Threads (std::thread uses pthread under the hood on Linux)
find_package(Threads REQUIRED)
target_link_libraries(stream_pipeline_stage0
    PRIVATE
        Threads::Threads
)

# Optional: stricter warnings (enable if you prefer)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(stream_pipeline_stage0 PRIVATE
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
    )
endif()
