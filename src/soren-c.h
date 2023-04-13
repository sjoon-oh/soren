#pragma once
/* github.com/sjoon-oh/soren
 * @author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#ifdef __cplusplus

#include <cstdint>
#include <cstddef>

//
// This is C-wrapper for Soren.
//  Functions below includes necessary features for replications.
// 
//  Wrapped functions of Soren starts with `cw`.
//

extern "C" {
#else

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#endif

void*   cwInitConnection(uint32_t);             // C-wrapper for connection initialization.
void    cwCleanConnection(void*);               

int     cwGetNumPlayers(void*);

void    cwInitSoren(uint32_t, uint32_t);        // Re-written in C, of soren::InitSoren().
void    cwPropose(uint8_t*, size_t, uint16_t);

void    cwCleanSoren();                         // Re-written in C, of soren::cleanSoren().

void    cwInitTs(int32_t);                      // Re-written in C, of soren::initTimestamps().
void    cwDumpTs();                             // did same, of soren::dumpElapsedTimes().

int32_t cwMarkTsBefore();
void    cwMarkTsAfter(int32_t);

#ifdef __cplusplus
}
#endif