#pragma once
/* github.com/sjoon-oh/soren
 * Author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include <cstdint>

namespace soren {

    struct __attribute__((packed)) Slot {
        uintptr_t   addr;
        uint32_t    size;
        uint32_t    canary;
    };

    struct __attribute__((packed)) SlotCanary {
        uint32_t    canary;
    };

    struct __attribute__(()) PlayerMessage {
        uint16_t    from_nid;
        uint16_t    to_nid;
        char        msg[32];
    };

    const bool isSlotValid(Slot&, uint32_t);

}