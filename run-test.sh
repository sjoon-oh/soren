#!/bin/bash

# github.com/sjoon-oh/soren
# Author: Sukjoon Oh, sjoon@kaist.ac.kr
# 
# Project SOREN
# 

rm *log

# ./build/soren-heartbeat.test
# ./build/soren-partitioner.test
# ./build/soren-connector.test

./make.sh
wait
./build/soren-$1.test