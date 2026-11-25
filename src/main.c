/**
 * @file main.c
 */

#include "AT32F403Axx.h"

#include "systick.h"

/**
 * @brief Bring the core from the reset-default HICK 8MHz up to HEXT*16 PLL.
 *
 * Overrides the weak no-op in the platform layer. Runs from
 * Reset_IRQHandler before .data/.bss init and before main(), so it must
 * touch nothing but CRM registers and compile-time constants.
 *
 * The sequence is dictated by the clock tree: start and stabilize the
 * crystal, configure and lock the PLL while the core still runs on HICK,
 * pre-scale every clock domain that has a speed ceiling below the new
 * SCLK, and only then switch SCLKSEL. Switching first would overclock
 * the APB domains for the duration of the switch.
 */
void SystemInit(void) {
    // Start the 12MHz crystal (HSE_VALUE) and wait for it to stabilize.
    // HICK keeps clocking the core throughout, so the busy-wait is safe.
    CRM->CTRL |= CRM_CTRL_HEXTEN_Msk;
    while (!(CRM->CTRL & CRM_CTRL_HEXTSTBL_Msk));

    // Configure the PLL for 192MHz while it is still off (PLLMULT and
    // friends must not change while PLLEN is set).
    //
    // 192 rather than the 240MHz maximum: USB is derived from the PLL by
    // an integer/fractional divider, and 192/4 = 48MHz exactly. Running
    // USB off the trimmed internal RC (HICK48) would drift with temp and
    // voltage; the crystal-derived PLL does not.
    CRM->CFG |= CRM_CFG_PLLRCS_Msk; // PLL entry = HEXT (not HICK/2)...
    CRM->CFG &= ~CRM_CFG_PLLHEXTDIV_Msk; // ...undivided: F_PLLIN = 12MHz.
    // Multiplier encoding: values 0..14 mean x2..x16, so 14 means x16.
    // (15 also means x16; 16..63 mean x17..x64 via PLLMULT5_4.)
    CRM->CFG = CRM->CFG & (~CRM_CFG_PLLMULT3_0_Msk) | (14 << CRM_CFG_PLLMULT3_0_Pos);
    CRM->CFG &= ~CRM_CFG_PLLMULT5_4_Msk; // 12MHz * 16 = 192MHz.
    CRM->CFG |= CRM_CFG_PLLRANGE_Msk; // PLL output range > 72MHz.
    CRM->CTRL |= CRM_CTRL_PLLEN_Msk;
    while (!(CRM->CTRL & CRM_CTRL_PLLSTBL_Msk)); // Wait for PLL lock.

    // Pre-scale the limited-speed domains BEFORE raising SCLK, so no bus
    // ever sees a frequency above its ceiling, even momentarily.
    // USBDIV/ADCDIV are split fields like PLLMULT; the encodings below are
    // the vendor's CRM_USB_DIV_4 (0b110) and CRM_ADC_DIV_16 (0b111).
    CRM->CFG |= CRM_CFG_USBDIV1_0_Msk | CRM_CFG_USBDIV2_Msk; // F_USB = PLL/4 = 48MHz (USB spec).
    // ADC divides APB2, not SCLK: 96MHz/8 = 12MHz (ceiling 28MHz).
    CRM->CFG |= CRM_CFG_ADCDIV1_0_Msk | CRM_CFG_ADCDIV2_Msk;
    // APB dividers: 0b100 means /2 (0xx = /1, 100..111 = /2 /4 /8 /16).
    // APB1/APB2 ceiling is 120MHz; 192/2 = 96MHz stays under it.
    CRM->CFG = CRM->CFG & (~CRM_CFG_APB1DIV_Msk) | (0b100 << CRM_CFG_APB1DIV_Pos);
    CRM->CFG = CRM->CFG & (~CRM_CFG_APB2DIV_Msk) | (0b100 << CRM_CFG_APB2DIV_Pos);
    // No flash wait-state programming here: unlike STM32's FLASH->ACR,
    // the F403A's zero-wait-state flash accelerator handles the frequency
    // change in hardware, and its FLASH->PSR has no software wait-cycle
    // field (the vendor BSP never writes it on this family).

    // Auto-step mode ramps SCLK to the new frequency in hardware steps
    // instead of one jump, so the core voltage regulator and the flash
    // accelerator keep up. The vendor BSP brackets every SCLK switch with
    // it; leaving it off risks a hard fault on the switch at high ratios.
    CRM->MISC3 |= CRM_MISC3_AUTO_STEP_EN_Msk;

    // Switch SCLK to the PLL and wait until the hardware reports the
    // switch complete (SCLKSTS, not SCLKSEL: the write only requests it).
    CRM->CFG = CRM->CFG & (~CRM_CFG_SCLKSEL_Msk) | (0b10 << CRM_CFG_SCLKSEL_Pos);
    while ((CRM->CFG & CRM_CFG_SCLKSTS_Msk) != (0b10 << CRM_CFG_SCLKSTS_Pos));

    CRM->MISC3 &= ~CRM_MISC3_AUTO_STEP_EN_Msk;

    // No SystemCoreClockUpdate() here: it writes a .data global, and
    // .data is not copied from flash until after SystemInit() returns.
    // Reset_IRQHandler calls it at the right time.
}

int main(void) {
    systick_init();

    for (;;) {
        delay(500);
    }
}
