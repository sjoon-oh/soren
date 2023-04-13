/* github.com/sjoon-oh/soren
 * Author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include <cstdint>
#include <cstring>

#include <fstream>
#include <iostream>

#include "timer.hh"

namespace soren {

    uint32_t    glob_ntimestamps;
    std::atomic<int32_t>    glob_tsidx;


    TIMESTAMP_T* GLOB_TS_BEFORE     = nullptr;
    TIMESTAMP_T* GLOB_TS_AFTER      = nullptr;

    uint64_t* GLOB_TS_ELAPSED       = nullptr;


    void initTimestamps(uint32_t arg_nts) {

        GLOB_TS_BEFORE = new TIMESTAMP_T[arg_nts];
        GLOB_TS_AFTER = new TIMESTAMP_T[arg_nts];

        std::memset(GLOB_TS_BEFORE, 0, sizeof(TIMESTAMP_T) * arg_nts);
        std::memset(GLOB_TS_AFTER, 0, sizeof(TIMESTAMP_T) * arg_nts);

        glob_ntimestamps = arg_nts;

        glob_tsidx.store(0);
    }

    void dumpElapsedTimes() {

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

    int32_t __MARK_TS_BEFORE__() {

        int32_t idx = glob_tsidx.fetch_add(1);
        GET_TIMESTAMP(GLOB_TS_BEFORE[idx]);

        return idx;
    }

    void __MARK_TS_AFTER__(int32_t arg_idx) {
        GET_TIMESTAMP(GLOB_TS_AFTER[arg_idx]);
    }
}
