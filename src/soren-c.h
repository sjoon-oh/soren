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

void    cwInitSoren(uint32_t, uint32_t);
void    cwPropose(uint8_t*, size_t, uint16_t);

void    cwCleanSoren();

void    cwInitTs(int32_t);
void    cwDumpTs();

int32_t cwMarkTsBefore();
void    cwMarkTsAfter(int32_t);

#ifdef __cplusplus
}
#endif