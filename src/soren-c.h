#pragma once
/* github.com/sjoon-oh/soren
 * Author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#ifdef __cplusplus

#include <cstdint>
#include <cstddef>

extern "C" {
#else

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#endif

void*   cwInitConnection(uint32_t);
void    cwCleanConnection(void*);

int     cwGetNumPlayers(void*);

void*   cwInitReplicator(uint32_t, void*);

#ifdef __cplusplus
}
#endif