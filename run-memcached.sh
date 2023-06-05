#!/bin/bash

# github.com/sjoon-oh/soren
# Author: Sukjoon Oh, sjoon@kaist.ac.kr
# 
# Project SOREN
# 

./clean-make-exp.sh

# ./build/memcached-soren.exp -vv -p 6379 -t 4
./build/memcached-soren.exp -p 6379 -t 4 -m 32768
# ./build/memcached-soren.exp -p 6379 -t 4 -m 32768