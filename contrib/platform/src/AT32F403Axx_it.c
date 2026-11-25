/**
 * @file AT32F403Axx_it.c
 *
 * Startup code in C for Cortex-M microcontrollers.
 *
 * Cortex-M hardware does not run code at reset the way a desktop CPU does.
 * Instead it reads two 32-bit words from address 0 (aliased to flash on this
 * part): word 0 is loaded into the stack pointer, word 1 is the address to
 * start executing. Those two words begin the "vector table", an array of
 * function pointers where slot N holds the handler for exception N. Slots
 * 0-15 are the architecturally fixed system exceptions (reset, NMI, faults,
 * SysTick, ...); slot 16 and up are the device-specific peripheral interrupts
 * listed in the SVD. This file builds that table and the reset routine that
 * brings the C environment up before main().
 *
 * The table is generated with the "X-Macro" pattern: AT32F403Axx_handlers.h
 * (auto-generated from the SVD by the irqn tool) contains nothing but calls
 * to two macros, e.g.
 *
 *     IRQN_HANDLER(0, WWDT)      // peripheral IRQ 0 is named WWDT
 *     SKIP_HANDLER(-9)           // reserved slot, no handler
 *
 * The file is #included several times below, each time with IRQN_HANDLER and
 * SKIP_HANDLER defined differently, so one list expands into the table
 * entries, then into handler declarations, etc. The list is never edited by
 * hand; regenerate the SVD-derived header instead.
 */

#include "AT32F403Axx_it.h"

#include "AT32F403Axx.h"
#include <string.h>

/* These are not real functions. __stack is a linker-provided symbol whose
 * *address* is the top of RAM (the initial stack pointer value); declaring it
 * as a function lets its address sit in a function-pointer table without a
 * cast. _start is the newlib C runtime entry: it runs constructors and calls
 * main(). */
void __stack(void);
void _start(void);
void Reset_IRQHandler(void);
static void Default_IRQHandler(void);

/* The vector table itself. The section attribute places it in .vector, which
 * the linker script keeps at the bottom of the flash image; "used" stops the
 * compiler from discarding it since nothing references it in C. Designated
 * initializers ([n] = ...) put each handler in its hardware-defined slot:
 * peripheral IRQ n lives at slot n + 16 because slots 0-15 are the system
 * exceptions. The list entry (-14, NonMaskableInt) thus lands at slot 2, and
 * (0, WWDT) at slot 16. Reserved slots get the default handler so a stray
 * interrupt is caught rather than executing garbage.
 *
 * (Comments here deliberately avoid spelling the two macro names: this list
 * is expanded while they are defined, and the preprocessor would expand them
 * even inside comments, corrupting the table.) */
__attribute__((used, section(".vector"))) vector_t vector[] = {
    [0] = __stack,
    [1] = Reset_IRQHandler,
#define IRQN_HANDLER(idx, name) [idx + 16] = name##_IRQHandler,
#define SKIP_HANDLER(idx) [idx + 16] = Default_IRQHandler,
#include "AT32F403Axx_handlers.h"
#undef IRQN_HANDLER
#undef SKIP_HANDLER
};

/* Second pass over the same list: declare every handler as a weak alias of
 * the default handler. "Weak" means the linker prefers any strong definition
 * elsewhere, so application code overrides a handler just by defining
 * void WWDT_IRQHandler(void) { ... } with no registration or table edits.
 * Without an override, the vector still points at a real function. */
#define IRQN_HANDLER(idx, name) __attribute__((weak, alias("Default_IRQHandler"))) void name##_IRQHandler(void);
#define SKIP_HANDLER(idx)
#include "AT32F403Axx_handlers.h"
#undef IRQN_HANDLER
#undef SKIP_HANDLER

/* Linker-script symbols marking where each section runs and where its
 * initial contents are stored in flash. Declared as arrays/objects so &sym
 * yields the address; the values themselves are never read. "load" is the
 * flash address (LMA), "start"/"end" bracket the RAM address (VMA). */
extern uint8_t __vector_start__;
extern uint8_t __vector_load__;
extern uint8_t __vector_end__;

extern uint8_t __data_start__;
extern uint8_t __data_load__;
extern uint8_t __data_end__;

extern uint8_t __bss_start__;
extern uint8_t __bss_end__;

void Reset_IRQHandler(void) {

    /* The FPU is off at reset; with -mfloat-abi=hard the very first float
     * instruction would fault. CPACR bits 20-23 grant full access to
     * coprocessors 10/11 (the FPU). DSB+ISB flush the pipeline so the enable
     * takes effect before any FP instruction executes. */
    SCB->CPACR |= 0xF << 10;
    __DSB(); __ISB();

    SystemInit(); // Initialize system clock

    /* RAM contents are undefined at power-up, so copy the flash images of
     * .vector and .data into RAM, then zero .bss (C requires uninitialized
     * globals to read as zero). The vector table is copied to RAM so VTOR
     * can point at it and handlers can be redirected at run time. */
    memcpy(&__vector_start__, &__vector_load__, &__vector_end__ - &__vector_start__);
    memcpy(&__data_start__, &__data_load__, &__data_end__ - &__data_start__);
    memset(&__bss_start__, 0, &__bss_end__ - &__bss_start__);

    /* VTOR (Vector Table Offset Register) tells the core where the table
     * lives; after this, interrupts fetch handlers from the RAM copy. */
    SCB->VTOR = (uint32_t)vector;

    SystemCoreClockUpdate();

    _start();

    /* main() returned (embedded main normally never does). Park the core
     * in low-power wait-for-event rather than spinning. */
    for (;;) __WFE();
}

/* Catch-all for unhandled interrupts: break into the debugger if one is
 * attached (BKPT is a no-op-ish hardfault without one), then park. */
static void Default_IRQHandler(void) {
    __BKPT();
    for (;;) __WFE();
}

/* newlib calls _exit() when main() returns or exit() is invoked; route it to
 * the same park loop. The pragma silences GCC's complaint that our _exit
 * declaration differs from its built-in prototype (which takes an int). */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wbuiltin-declaration-mismatch"
__attribute__((weak, alias("Default_IRQHandler"))) void _exit(void);
#pragma GCC diagnostic pop
