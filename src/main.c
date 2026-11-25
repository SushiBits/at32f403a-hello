/**
 * @file main.c
 */

#include "AT32F403Axx.h"

#include "systick.h"

int main(void) {
    systick_init();

    for (;;) {
        delay(500);
    }
}
