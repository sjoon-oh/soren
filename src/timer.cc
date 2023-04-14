/* github.com/sjoon-oh/soren
 * @author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include <cstdint>
#include <cstring>

#include <fstream>
#include <iostream>

#include "timer.hh"

// This source comes from the repository Mu (https://github.com/LPD-EPFL/mu/)
// The function provides waays how to measure delays by setting before-after 
// timestamp.

namespace soren {

    uint32_t    glob_ntimestamps;       // This decides how many indices there should be.
    std::atomic<int32_t>    glob_tsidx; // This blocks arbitrary writes of the same index by multiple threads.


    TIMESTAMP_T* GLOB_TS_BEFORE     = nullptr;
    TIMESTAMP_T* GLOB_TS_AFTER      = nullptr;

    uint64_t* GLOB_TS_ELAPSED       = nullptr;
}



/// @brief Initializes timestamp arrays
/// @param arg_nts 
void soren::initTimestamps(uint32_t arg_nts) {

    GLOB_TS_BEFORE = new TIMESTAMP_T[arg_nts];
    GLOB_TS_AFTER = new TIMESTAMP_T[arg_nts];

    std::memset(GLOB_TS_BEFORE, 0, sizeof(TIMESTAMP_T) * arg_nts);
    std::memset(GLOB_TS_AFTER, 0, sizeof(TIMESTAMP_T) * arg_nts);

    glob_ntimestamps = arg_nts;

    glob_tsidx.store(0);
}



/// @brief Exports saved time differences.
void soren::dumpElapsedTimes() {

    // 
    // Exports saved time differences. The filename is fixed to 'soren-dump.txt'
    // run-test.sh file renames this file by inserting the datetime of the system.
    // Refer to the run-test.sh.

    GLOB_TS_ELAPSED = new uint64_t[glob_ntimestamps];

    for (int idx = 0; idx < glob_ntimestamps; idx++)
        GLOB_TS_ELAPSED[idx] = ELAPSED_NSEC(GLOB_TS_BEFORE[idx], GLOB_TS_AFTER[idx]);

    std::ofstream dump_file;
    dump_file.open("soren-dump.txt");
    
    for (int idx = 0; idx < glob_ntimestamps; idx++)
        if (GLOB_TS_ELAPSED[idx] != 0) 
            dump_file << GLOB_TS_ELAPSED[idx] << std::endl;

    dump_file.close();

    delete[] GLOB_TS_ELAPSED;
}



/// @brief Record BEFORE timestamp.
/// @return 
int32_t soren::__MARK_TS_BEFORE__() {

    int32_t idx = glob_tsidx.fetch_add(1);
    GET_TIMESTAMP(GLOB_TS_BEFORE[idx]);

    return idx;
}



/// @brief Record AFTER timestamp.
/// @param arg_idx 
void soren::__MARK_TS_AFTER__(int32_t arg_idx) {
    GET_TIMESTAMP(GLOB_TS_AFTER[arg_idx]);
}
