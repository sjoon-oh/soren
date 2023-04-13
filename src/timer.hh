#pragma once
/* github.com/sjoon-oh/soren
 * Author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include <inttypes.h>
#include <time.h>

#include <atomic>

/*
USAGE:
    TIMESTAMP_INIT
    TIMESTAMP_T t1, t2;

    GET_TIMESTAMP(t1);
    GET_TIMESTAMP(t2);
    printf("Elapsed time in nanoseconds: %lu\n", ELAPSED_NSEC(t1, t2));
*/

#define TIMESTAMP_INIT      do {} while(0)
#define TIMESTAMP_T         struct timespec
#define GET_TIMESTAMP(t)    clock_gettime(CLOCK_MONOTONIC, &t)
#define ELAPSED_NSEC(t1, t2) (t2.tv_nsec + t2.tv_sec * 1000000000UL - t1.tv_nsec - t1.tv_sec * 1000000000UL)

namespace soren {

    void initTimestamps(uint32_t);
    void dumpElapsedTimes();

    int32_t __MARK_TS_BEFORE__();
    void __MARK_TS_AFTER__(int32_t);
}