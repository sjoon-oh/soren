#pragma once
/* github.com/sjoon-oh/soren
 * @author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include <cstdint>
#include <ctime>

namespace soren {

    const uint32_t REPLAYER_READY = 0xdeadface;

    enum {
        FOOTPRINT_INSERTED,
        FOOTPRINT_SWITCHED,
        FOOTPRINT_INVALID,
    };

    struct __attribute__((packed)) LogStat {
        uint32_t    n_prop;                     // Current proposal number
        uint32_t    offset;                     // MR buffer offset.
        uint32_t    dummy1;
        uint32_t    dummy2;
    };

    struct __attribute__((packed)) RequestSlot {
        uint8_t     req_a;
        uint8_t     req_b;
        uint8_t     req_c;
        uint8_t     req_d;
    };

    struct __attribute__((packed)) HeaderSlot {
        struct RequestSlot      reqs;
        uint32_t                n_prop;
        uintptr_t               addr;
        uint16_t                size;
        uint16_t                owner;          // Who owns this key?
        int32_t                 canary;
    };

    struct LocalSlot {
        struct HeaderSlot   header;
        struct LocalSlot*   next_slot;
        
        uint32_t            hashed_key;
        struct timespec     timestamp;
        uint8_t             footprint;
    };

    struct __attribute__((packed)) SlotCanary {
        int32_t    canary;
    };

    const bool isSlotValid(HeaderSlot&, uint32_t);

    int localSlotTsComp(void*, void*);
    int localSlotHashComp(void*, void*);
}