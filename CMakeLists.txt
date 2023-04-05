# SOREN
# Author: Sukjoon Oh, sjoon@kaist.ac.kr

# Global settings
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

#
# Hartebeest subdir
add_subdirectory(
    ${CMAKE_SOURCE_DIR}/hartebeest
    ${CMAKE_SOURCE_DIR}/hartebeest/build
    )

include_directories(${CMAKE_SOURCE_DIR}/hartebeest/src)

# External dependencies.
include_directories(${CMAKE_SOURCE_DIR}/extern)

#
# --------------------------------------
# Target: SOREN_TARGET_DEMO
# Demo Executable Settings
set(SOREN_TARGET_DEMO
    soren-demo.run)
file(GLOB SOREN_TARGET_DEMO_SRCFILES 
    CONFIGURE_DEPENDS 
    ${CMAKE_SOURCE_DIR}/src/test/demo.cc
    ${CMAKE_SOURCE_DIR}/src/*.cc 
    ${CMAKE_SOURCE_DIR}/src/*.hh)

add_executable(
    ${SOREN_TARGET_DEMO}
    ${SOREN_TARGET_DEMO_SRCFILES}
)
set_target_properties(
    ${SOREN_TARGET_DEMO} 
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build
)

# Library Link
target_link_directories(${SOREN_TARGET_DEMO} PUBLIC "build/out")
target_link_libraries(
    ${SOREN_TARGET_DEMO}  
    PUBLIC ibverbs pthread hartebeest)


#
# --------------------------------------
# Target: SOREN_TEST_HEARTBEAT
set(SOREN_TEST_HEARTBEAT
    soren-heartbeat.test)
file(GLOB SOREN_TEST_HEARTBEAT_SRCFILES 
    CONFIGURE_DEPENDS 
    ${CMAKE_SOURCE_DIR}/src/test/test-heartbeat.cc
    ${CMAKE_SOURCE_DIR}/src/*.cc 
    ${CMAKE_SOURCE_DIR}/src/*.hh)

add_executable(
    ${SOREN_TEST_HEARTBEAT}  
    ${SOREN_TEST_HEARTBEAT_SRCFILES}
)
set_target_properties(
    ${SOREN_TEST_HEARTBEAT} 
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build
)

target_link_libraries(
    ${SOREN_TEST_HEARTBEAT}  
    PUBLIC pthread)
