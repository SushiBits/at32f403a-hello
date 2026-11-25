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

#ifndef HSE_VALUE
#define HSE_VALUE 8000000
#endif

#ifndef LSE_VALUE
#define LSE_VALUE 32768
#endif

__attribute__((weak)) void SystemInit(void) {}

void SystemCoreClockUpdate(void) {
    uint32_t SCLKSTS = (CRM->CFG & CRM_CFG_SCLKSTS_Msk) >> CRM_CFG_SCLKSTS_Pos;
    uint32_t PLLMULT =
        ((CRM->CFG & CRM_CFG_PLLMULT3_0_Msk) >> CRM_CFG_PLLMULT3_0_Pos) |
        ((CRM->CFG & CRM_CFG_PLLMULT5_4_Msk) >> (CRM_CFG_PLLMULT5_4_Pos - 4));
    uint32_t AHBDIV = (CRM->CFG & CRM_CFG_AHBDIV_Msk) >> CRM_CFG_AHBDIV_Pos;

    switch (SCLKSTS) {
        case 0b01: // HEXT
            SystemCoreClock = HSE_VALUE;
            break;

        case 0b10: // PLL
            // PLL Source Select
            if (CRM->CFG & CRM_CFG_PLLRCS_Msk) { // HEXTDIV
                SystemCoreClock = HSE_VALUE;
                if (CRM->CFG & CRM_CFG_PLLHEXTDIV_Msk) { // HEXT is divided
                    uint32_t HEXTDIV = (CRM->MISC3 & CRM_MISC3_HEXTDIV_Msk) >> CRM_MISC3_HEXTDIV_Pos;
                    SystemCoreClock /= (HEXTDIV + 2);
                }
            } else { // HICK4M
                SystemCoreClock = 4000000;
            }

            // PLL Multiplier
            uint32_t multiplier = PLLMULT + ((PLLMULT < 15) ? 2 : 1);
            SystemCoreClock *= multiplier;
            break;

        case 0b00: // HICK
        default:
            // 48MHz reaches SCLK only with both HICK_TO_SCLK and HICKDIV set;
            // HICKDIV alone just feeds HICK/6 (8MHz) into the clock tree.
            if ((CRM->MISC3 & CRM_MISC3_HICK_TO_SCLK_Msk) &&
                (CRM->MISC1 & CRM_MISC1_HICKDIV_Msk)) {
                SystemCoreClock = 48000000;
            } else {
                SystemCoreClock = 8000000;
            }
            break;
    }

    // AHBDIV 0xxx: no division; 1000..1011: /2 /4 /8 /16 (shift 1..4);
    // 1100..1111: /64 /128 /256 /512 (shift 6..9). Bit 2 skips the missing
    // /32 encoding, so the shift is low2 + (bit2 ? 6 : 1), gated by bit 3.
    //
    // Bit math rather than a ROM table because of the flash accelerator's
    // single-line cache: the linker places .rodata after all of .text, so in
    // the linked image the table sits far from this code and its load is a
    // guaranteed cache miss that also evicts the code line being prefetched.
    // The expression fetches only instructions; at -O2 it is branchless IT
    // blocks, and even the short jumps GCC outlines at -Og stay within this
    // function, i.e. the same or adjacent cache line.
    if (AHBDIV & 0x8) {
        uint32_t shift = (AHBDIV & 0x3) + ((AHBDIV & 0x4) ? 6 : 1);
        SystemCoreClock >>= shift;
    }
}
