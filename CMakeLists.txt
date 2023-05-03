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
    ${CMAKE_SOURCE_DIR}/src/*.c
    ${CMAKE_SOURCE_DIR}/src/*.cc 
    ${CMAKE_SOURCE_DIR}/src/*.hh)

add_library(${LIBSOREN} SHARED ${LIBSOREN_SRCFILES})
# add_library(${LIBSOREN}-static STATIC ${LIBSOREN_SRCFILES})
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
# Target: SOREN_TEST_HEARTBEAT
# set(SOREN_TEST_HEARTBEAT ${SOREN_PREFIX}-heartbeat.${SOREN_SUFFIX_TEST})
# file(GLOB SOREN_TEST_HEARTBEAT_SRCFILES 
#     CONFIGURE_DEPENDS 
#     ${CMAKE_SOURCE_DIR}/src/test/test-heartbeat.cc
# )

# add_executable(
#     ${SOREN_TEST_HEARTBEAT}
#     ${SOREN_TEST_HEARTBEAT_SRCFILES}
# )
# target_link_libraries(${SOREN_TEST_HEARTBEAT} PUBLIC ${LIBSOREN})


#
# --------------------------------------
# Target: SOREN_TEST_CONNECTOR
# set(SOREN_TEST_CONNECTOR
#     ${SOREN_PREFIX}-connector.${SOREN_SUFFIX_TEST})
#     file(GLOB SOREN_TEST_CONNECTOR_SRCFILES 
#     CONFIGURE_DEPENDS 
#     ${CMAKE_SOURCE_DIR}/src/test/test-connector.cc
# )

# add_executable(
#     ${SOREN_TEST_CONNECTOR}  
#     ${SOREN_TEST_CONNECTOR_SRCFILES}
# )
# target_link_libraries(${SOREN_TEST_CONNECTOR} PUBLIC ${LIBSOREN})

#
# --------------------------------------
# Target: SOREN_TEST_DEMO
set(SOREN_TEST_DEMO
    ${SOREN_PREFIX}-demo.${SOREN_SUFFIX_TEST})
file(GLOB SOREN_TEST_DEMO_SRCFILES 
    CONFIGURE_DEPENDS 
    ${CMAKE_SOURCE_DIR}/src/test/test-demo.cc
)

add_executable(
    ${SOREN_TEST_DEMO}  
    ${SOREN_TEST_DEMO_SRCFILES}
)
target_link_libraries(${SOREN_TEST_DEMO} PUBLIC ${LIBSOREN})

#
# --------------------------------------
# Target: SOREN_TEST_DEMO
set(SOREN_TEST_DEMO_SP
    ${SOREN_PREFIX}-demo-subpar.${SOREN_SUFFIX_TEST})
file(GLOB SOREN_TEST_DEMO_SP_SRCFILES 
    CONFIGURE_DEPENDS 
    ${CMAKE_SOURCE_DIR}/src/test/test-demo-subpar.cc
)

add_executable(
    ${SOREN_TEST_DEMO_SP}  
    ${SOREN_TEST_DEMO_SP_SRCFILES}
)
target_link_libraries(${SOREN_TEST_DEMO_SP} PUBLIC ${LIBSOREN})

#
# --------------------------------------
# Target: SOREN_HASHTABLE_TEST_ST (Single Threaded Test)
set(SOREN_TEST_HASHTABLE_ST
    ${SOREN_PREFIX}-hashtable-st.${SOREN_SUFFIX_TEST})
file(GLOB SOREN_TEST_HASHTABLE_ST_SRCFILES 
    CONFIGURE_DEPENDS 
    ${CMAKE_SOURCE_DIR}/src/test/test-hashtable-st.cc
)

add_executable(
    ${SOREN_TEST_HASHTABLE_ST}  
    ${SOREN_TEST_HASHTABLE_ST_SRCFILES}
)
target_link_libraries(${SOREN_TEST_HASHTABLE_ST} PUBLIC ${LIBSOREN})

#
# --------------------------------------
# Target: SOREN_HASHTABLE_TEST_MT (Multi-threaded Test)
set(SOREN_TEST_HASHTABLE_MT
    ${SOREN_PREFIX}-hashtable-mt.${SOREN_SUFFIX_TEST})
file(GLOB SOREN_TEST_HASHTABLE_MT_SRCFILES 
    CONFIGURE_DEPENDS 
    ${CMAKE_SOURCE_DIR}/src/test/test-hashtable-mt.cc
)

add_executable(
    ${SOREN_TEST_HASHTABLE_MT}  
    ${SOREN_TEST_HASHTABLE_MT_SRCFILES}
)
target_link_libraries(${SOREN_TEST_HASHTABLE_MT} PUBLIC ${LIBSOREN})


#
# --------------------------------------
# Target: SOREN_EXP_STANDALONE (Standalone Test)
set(SOREN_EXP_STANDALONE
    soren-standalone.demo)
file(GLOB SOREN_EXP_STANDALONE_SRCFILES
    CONFIGURE_DEPENDS 
    ${CMAKE_SOURCE_DIR}/src/standalone/exp-standalone.cc
)

add_executable(
    ${SOREN_EXP_STANDALONE}  
    ${SOREN_EXP_STANDALONE_SRCFILES}
)
target_link_libraries(${SOREN_EXP_STANDALONE} PUBLIC ${LIBSOREN})
