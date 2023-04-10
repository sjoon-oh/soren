#!/bin/bash

# github.com/sjoon-oh/soren
# Author: Sukjoon Oh, sjoon@kaist.ac.kr
# 
# Project SOREN
# 

PAR=$(cat /proc/cpuinfo | grep cores | wc -l)
cmake -S . -B build
# make -f build/Makefile libhartebeest

cd ./build && make -j 16 hartebeest

if [ ! -d "out" ]; then
    mkdir -p out
fi

cp ../hartebeest/build/libhartebeest.so ./out
make -j 16 all

rm ../*log
rm ../*json