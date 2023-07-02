#!/bin/bash

# github.com/sjoon-oh/soren
# Author: Sukjoon Oh, sjoon@kaist.ac.kr
# 
# Project SOREN
# 

# Remove previous Redis persistence
rm ./dump.rdb

./build/redis-soren.exp \
    --port 6379 \
    --protected-mode no \
    --io-threads-do-reads yes \
    --save "" --appendonly no
    # --io-threads 4