/**
 * @file AT32F403Axx_it.h
 *
 * Public interface to the startup code in AT32F403Axx_it.c: the interrupt
 * vector table and one prototype per device interrupt handler.
 *
 * To handle a peripheral interrupt, just define the matching function in
 * application code, e.g. void SysTick_IRQHandler(void) { ... }. The startup
 * code declares every handler as a weak alias of a default park loop, so a
 * strong definition anywhere in the link silently replaces it — there is no
 * registration call and no table to edit.
 */

#ifndef _AT32F403Axx_IT_H_
#define _AT32F403Axx_IT_H_

#include <sys/cdefs.h>

/* One vector table slot: the hardware loads a 32-bit address from the table
 * and branches to it. Slot 0 is actually the initial stack pointer value,
 * not a function, but it is the same width; a function pointer type keeps
 * the table uniform. */
typedef void (*vector_t)(void);

__BEGIN_DECLS

/* The vector table, built in AT32F403Axx_it.c and copied to RAM at reset.
 * Exposed so applications can inspect or redirect entries at run time. */
extern vector_t vector[];

/* X-Macro expansion (see AT32F403Axx_it.c for the full explanation):
 * AT32F403Axx_handlers.h is a generated list of IRQN_HANDLER(idx, name) /
 * SKIP_HANDLER(idx) invocations. Defining the macros as prototypes and
 * including the list turns it into declarations of name##_IRQHandler for
 * every real interrupt; reserved slots (SKIP_HANDLER) declare nothing.
 * The macros are undefined afterwards so the list can be expanded again
 * with different definitions elsewhere. */
#define IRQN_HANDLER(idx, name) void name##_IRQHandler(void);
#define SKIP_HANDLER(idx)
#include "AT32F403Axx_handlers.h"
#undef IRQN_HANDLER
#undef SKIP_HANDLER

__END_DECLS

#endif // defined(_AT32F403Axx_IT_H_)