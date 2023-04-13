#pragma once
/* github.com/sjoon-oh/soren
 * Author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

// This source comes from the repository Mu (https://github.com/LPD-EPFL/mu/)
// The function provides waays how to measure delays by setting before-after 
// timestamp.

#include <inttypes.h>
#include <time.h>

#include <atomic>

#define TIMESTAMP_INIT      do {} while(0)
#define TIMESTAMP_T         struct timespec
#define GET_TIMESTAMP(t)    clock_gettime(CLOCK_MONOTONIC, &t)
#define ELAPSED_NSEC(t1, t2) (t2.tv_nsec + t2.tv_sec * 1000000000UL - t1.tv_nsec - t1.tv_sec * 1000000000UL)

namespace soren {

    void initTimestamps(uint32_t);      // Initialize timestamp arrays
    void dumpElapsedTimes();            // Dump the time difference (after-before TS)

    int32_t __MARK_TS_BEFORE__();       
    void __MARK_TS_AFTER__(int32_t);    
}