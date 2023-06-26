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
REDIS_SRCDIR="redis-7.0.5"
REDIS_SRCFILE="redis-7.0.5.tar.gz"

echo "Current workspace set to: ${WKRSPACE_HOME}"
echo "****** soren build. ******"

rm build/*.exp

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

# C-Integrator Compile Test
gcc -o soren-demo-c.test ../src/test/test-demo-c.c -lsoren -L ./out

cd out
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:`pwd`
export LIBRARY_PATH=`pwd`:${LIBRARY_PATH}

echo "****** soren build end. ******"
echo "****** memcached-soren build. ******"


# Memcached Build
cd ${WKRSPACE_HOME}
cd ${MEMCACHED_DIR}

if [ -d "${MEMCACHED_SRCDIR}" ]; then
    rm -r ${MEMCACHED_SRCDIR}
fi

if [ -f "${WRKSPACE_HOME}/build/memcached-soren.exp" ]; then
    rm -r "${WRKSPACE_HOME}/build/memcached-soren.exp"
fi

tar -xf ${MEMCACHED_SRCFILE}

cd ${MEMCACHED_SRCDIR}
./configure
sed -i "s/LIBS = -lhugetlbfs -levent/LIBS = -lhugetlbfs -levent\nLIBS += -lsoren -lhartebeest/" Makefile

cp memcached.c memcached-orig.c
# cp items.c items-orig.c

cd ..
patch -p0 -d ${MEMCACHED_SRCDIR} < memcached-soren.patch

cd ${MEMCACHED_SRCDIR}
make -j
cp memcached ${WKRSPACE_HOME}/build/memcached-soren.exp

echo "****** memcached-soren build end. ******"

cd ${WKRSPACE_HOME}

echo "****** redis-soren build. ******"

# Redis Build
cd ${REDIS_DIR}

if [ -d "${REDIS_SRCDIR}" ]; then
    rm -r ${REDIS_SRCDIR}
fi

if [ -f "${WRKSPACE_HOME}/build/redis-soren.exp" ]; then
    rm -r "${WRKSPACE_HOME}/build/redis-soren.exp"
fi

tar -xf ${REDIS_SRCFILE}
# cp make-patch.sh ./${REDIS_SRCDIR}

cd ${REDIS_SRCDIR}
# cp src/Makefile src/Makefile-orig
# cp src/networking.c src/networking-orig.c
cp src/server.c src/server-orig.c
cp src/server.h src/server-orig.h

cp src/sds.c src/sds-orig.c
cp src/sds.h src/sds-orig.h

sed -i "s|FINAL_LIBS=-lm|FINAL_LIBS=-lm\nFINAL_LIBS += -lsoren -lhartebeest -L ${WKRSPACE_HOME}/build/out |" src/Makefile

cd ..
patch -p1 -d ${REDIS_SRCDIR}/src < redis-soren.patch

cd ${REDIS_SRCDIR}
make -j 4
cp src/redis-server ${WKRSPACE_HOME}/build/redis-soren.exp

echo "****** redis-soren build end. ******"

