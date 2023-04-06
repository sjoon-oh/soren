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
# Main SOREN proj.
set(LIBSOREN    soren)
file(GLOB LIBSOREN_SRCFILES 
    CONFIGURE_DEPENDS 
    ${CMAKE_SOURCE_DIR}/src/*.cc 
    ${CMAKE_SOURCE_DIR}/src/*.hh)

add_library(${LIBSOREN} SHARED ${LIBSOREN_SRCFILES})
set_target_properties(
    ${LIBSOREN} 
    PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build/out
)
target_link_libraries(${LIBSOREN} PUBLIC ibverbs pthread hartebeest)

#
# Name prefix, suffix
set(SOREN_PREFIX        soren)
set(SOREN_SUFFIX_TEST   test)


#
# --------------------------------------
# Target: SOREN_TARGET_DEMO
# Demo Executable Settings
set(SOREN_TARGET_DEMO ${SOREN_PREFIX}-demo.run)
file(GLOB SOREN_TARGET_DEMO_SRCFILES 
    CONFIGURE_DEPENDS 
    ${CMAKE_SOURCE_DIR}/src/test/demo.cc
)

add_executable(
    ${SOREN_TARGET_DEMO}
    ${SOREN_TARGET_DEMO_SRCFILES}
)
target_link_libraries(${SOREN_TARGET_DEMO} PUBLIC ${LIBSOREN})

#
# --------------------------------------
# Target: SOREN_TEST_HEARTBEAT
set(SOREN_TEST_HEARTBEAT ${SOREN_PREFIX}-heartbeat.${SOREN_SUFFIX_TEST})
file(GLOB SOREN_TEST_HEARTBEAT_SRCFILES 
    CONFIGURE_DEPENDS 
    ${CMAKE_SOURCE_DIR}/src/test/test-heartbeat.cc
)

add_executable(
    ${SOREN_TEST_HEARTBEAT}
    ${SOREN_TEST_HEARTBEAT_SRCFILES}
)
target_link_libraries(${SOREN_TEST_HEARTBEAT} PUBLIC ${LIBSOREN})

#
# --------------------------------------
# Target: SOREN_TEST_PARTITION
set(SOREN_TEST_PARTITION
    ${SOREN_PREFIX}-partitioner.${SOREN_SUFFIX_TEST})
    file(GLOB SOREN_TEST_PARTITION_SRCFILES 
    CONFIGURE_DEPENDS 
    ${CMAKE_SOURCE_DIR}/src/test/test-heartbeat.cc
)

add_executable(
    ${SOREN_TEST_PARTITION}  
    ${SOREN_TEST_PARTITION_SRCFILES}
)
target_link_libraries(${SOREN_TEST_PARTITION} PUBLIC ${LIBSOREN})


#
# --------------------------------------
# Target: SOREN_TEST_CONNECTOR
set(SOREN_TEST_CONNECTOR
    ${SOREN_PREFIX}-connector.${SOREN_SUFFIX_TEST})
    file(GLOB SOREN_TEST_CONNECTOR_SRCFILES 
    CONFIGURE_DEPENDS 
    ${CMAKE_SOURCE_DIR}/src/test/test-connector.cc
)

add_executable(
    ${SOREN_TEST_CONNECTOR}  
    ${SOREN_TEST_CONNECTOR_SRCFILES}
)
target_link_libraries(${SOREN_TEST_CONNECTOR} PUBLIC ${LIBSOREN})

