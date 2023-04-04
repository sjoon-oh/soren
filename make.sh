#!/bin/bash

# github.com/sjoon-oh/soren
# Author: Sukjoon Oh, sjoon@kaist.ac.kr
# 
# Project SOREN
# 


cmake -S . -B build
# make -f build/Makefile libhartebeest

cd ./build && make -j 32

if [ ! -d "out" ]; then
    mkdir -p out
fi

cp ../hartebeest/build/libhartebeest.so ./out

