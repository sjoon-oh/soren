#pragma once
/* github.com/sjoon-oh/soren
 * Author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include <cstdint>

namespace soren {

    const uint8_t   SLOT_CANARY = 0xdeadceed;

    struct __attribute__((packed)) Slot {
        uint32_t    size;
        uint8_t*    addr;
        uint32_t    canary;
    };

    struct __attribute__(()) PlayerMessage {
        uint16_t    from_nid;
        uint16_t    to_nid;
        char        msg[32];
    };

    const bool isSlotValid(Slot& arg_tar) { return arg_tar.canary == SLOT_CANARY; }

}