/**
 * @file system_AT32F403xx.c
 *
 * System clock accounting for the AT32F403A family.
 *
 * SystemInit and the SystemCoreClock value are supplied by the application.
 * This is that layer, kept separate from the startup code in AT32F403Axx_it.c
 * so the latter stays family-independent.
 */

#include "AT32F403Axx.h"
#include "system_AT32F403Axx.h"

uint32_t SystemCoreClock;

__attribute__((weak)) void SystemInit(void) {}
void SystemCoreClockUpdate(void) {
}
