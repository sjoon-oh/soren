#pragma once
/* github.com/sjoon-oh/soren
 * Author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include <cstdint>

namespace soren {

    const uint32_t REPLAYER_READY = 0xdeadface;

    struct __attribute__((packed)) LogStat {
        uint64_t    n_prop;     // Current proposal number
        uint32_t    offset;     // MR buffer offset.
        uint32_t    dummy1;
        uint32_t    dummy2;
    };

    struct __attribute__((packed)) Slot {
        uint32_t    n_prop;
        uintptr_t   addr;
        uint32_t    size;
        int32_t    canary;
    };

    struct __attribute__((packed)) SlotCanary {
        int32_t    canary;
    };

    const bool isSlotValid(Slot&, uint32_t);

}