#ifndef _KERNAL_H_
#define _KERNAL_H_

#include "helpers.h"
//KernalFunctions
#define SCREEN_SET_CHAR_SET_ADDRESS 0xFF62
//Args
#define ISO 1
#define SCREEN_SET_CHAR_SET(CHARSET) \
    do { \
        asm("lda #%w", CHARSET); \
        asm("jsr %w", SCREEN_SET_CHAR_SET_ADDRESS); \
    } while(0)

#endif

#define GETIN_ADDRESS 0xFFE4

#if defined(LOGIC_APP_REPLAY_TARGET)
extern int logicAppReplayReadInput(int *output);
extern int logicAppReplayWaitForInput(int *output);
#endif

#if defined(LOGIC_APP_REPLAY_TARGET)
#define GET_IN(output) \
do { \
        /* The replay queue is a nonblocking GETIN source. A real KERNAL \
           GETIN call can wait forever when no key is queued. */ \
        int _logicAppReplayInputValue; \
        output = logicAppReplayReadInput(&_logicAppReplayInputValue) \
                     ? _logicAppReplayInputValue : 0; \
} while (0)
#define GET_IN_WAIT(output) \
do { \
        int _logicAppReplayInputValue; \
        while (!logicAppReplayWaitForInput(&_logicAppReplayInputValue)) { } \
        output = _logicAppReplayInputValue; \
} while (0)
#else
#define GET_IN(output) \
do { \
        asm("jsr %w", GETIN_ADDRESS); \
        asm("sta %v", _assmByte); \
        output = _assmByte; \
} while (0)
#define GET_IN_WAIT(output) GET_IN(output)
#endif
