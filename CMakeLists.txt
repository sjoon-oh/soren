# SOREN
# Author: Sukjoon Oh, sjoon@kaist.ac.kr

cmake_minimum_required(VERSION 3.10)
project(
    SOREN
    LANGUAGES   CXX   
    VERSION     0.0
    )

message("Processing SOREN CMakeLists.txt")

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_VERBOSE_MAKEFILE true)

# External dependencies.
include_directories(${CMAKE_SOURCE_DIR}/extern)

# Test Target
set(SOREN_TARGET_DEMO soren-demo.run)

# SOREN Demo Executable
add_executable(
    ${SOREN_TARGET_DEMO}
    ${CMAKE_SOURCE_DIR}/src/test/demo.cc 
)
set_target_properties(
    ${SOREN_TARGET_DEMO} 
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build
)

# Essentials
target_link_libraries(${SOREN_TARGET_DEMO}  
    PUBLIC ibverbs pthread)

#
# Hartebeest subdir
add_subdirectory(
    ${CMAKE_SOURCE_DIR}/hartebeest
    ${CMAKE_SOURCE_DIR}/hartebeest/build
    )