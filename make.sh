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

# mv ./libsoren-static.a ./out/
# mv ./out/libsoren-static.a ./out/libsoren.a

rm ../*log
rm ../*json

# C-Integrator Compile Test
# cd ../src/test
gcc -o soren-demo-c.test ../src/test/test-demo-c.c -lsoren -L ./out

cd out
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:`pwd`

cd ../..

echo "Added `pwd` to LD_LIBRARY_PATH.