#!/bin/bash

# github.com/sjoon-oh/soren
# Author: Sukjoon Oh, sjoon@kaist.ac.kr
# 
# Project SOREN
# 

WKRSPACE_HOME=`pwd`

MEMCACHED_DIR="./experiments/memcached"
MEMCACHED_SRCDIR="memcached-1.5.19"
MEMCACHED_SRCFILE="memcached-1.5.19.tar.gz"

REDIS_DIR="./experiments/redis"
REDIS_SRCFILE="redis-7.0.5.tar.gz"

rm build/*.exp

rm ./*log
rm ./*json

# Memcached Build
cd ${WKRSPACE_HOME}
cd ${MEMCACHED_DIR}

if [ -d "${MEMCACHED_SRCDIR}" ]; then
    rm -r ${MEMCACHED_SRCDIR}
fi

cd ${WKRSPACE_HOME}

