/**
 * @file AT32F403Axx_it.h
 */

#ifndef _AT32F403Axx_IT_H_
#define _AT32F403Axx_IT_H_

#include <sys/cdefs.h>

typedef void (*vector_t)(void);

__BEGIN_DECLS

extern vector_t vector[];

#define IRQN_HANDLER(idx, name) void name##_IRQHandler(void);
#define SKIP_HANDLER(idx)
#include "AT32F403Axx_handlers.h"
#undef IRQN_HANDLER
#undef SKIP_HANDLER

__END_DECLS

#endif // defined(_AT32F403Axx_IT_H_)