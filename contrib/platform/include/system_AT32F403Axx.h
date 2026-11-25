/**
 * @file system_AT32F403Axx.h
 * @author Max Chan <max@maxchan.info> with Copilot
 */

#ifndef SYSTEM_AT32F403AXX_H
#define SYSTEM_AT32F403AXX_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The device header SVDConv generates includes this file by name, so it has to
   sit next to it. The definitions belong to the platform layer. */

extern uint32_t SystemCoreClock;

void SystemInit(void);
void SystemCoreClockUpdate(void);

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_AT32F403AXX_H */
