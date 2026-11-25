# at32f403a-hello

Bare-metal project starter for the **Artery AT32F403ACC** (Cortex-M4F,
256 KB flash, 96 KB SRAM), built with `arm-none-eabi-gcc` and GNU Make.
No vendor SDK: the device header is generated from SVD, the startup code
is plain C, and the only libraries are CMSIS core headers and newlib.

The demo brings the core up to **192 MHz** from a 12 MHz crystal and
provides an Arduino-style SysTick timebase (`millis`, `micros`, `delay`,
`delay_us`).

## Layout

```
src/                    Application: main.c (clock bring-up), systick.c
include/                Application headers
lib/at32f403a-hello.ld  Linker script (vector + data in SRAM, code in flash)
contrib/platform/       Device support layer
  share/AT32F403Axx.svd   Peripheral description (source of truth)
  src/AT32F403Axx_it.c    C startup: vector table, reset handler
  src/system_AT32F403xx.c SystemCoreClock accounting from CRM registers
  lib/AT32F403ACC.ld      Memory map fragment (INCLUDEd by the app script)
  include/                Hand-written headers + generated headers (ignored)
contrib/cmsis/          CMSIS_6 submodule (core headers only, v6.4.0)
contrib/svdconv/        Submodule: SVD -> device header generator
contrib/irqn/           Submodule: device header -> IRQ handler table
```

Generated files (`AT32F403Axx.h`, `AT32F403Axx_handlers.h`) are build
products, not checked in. Edit the SVD, not the headers.

## Building

Requirements:

- `arm-none-eabi-gcc` toolchain (newlib), `make`, `python3` on PATH
- Submodules initialized: `git submodule update --init --recursive`

```sh
make            # generate headers + build at32f403a-hello.elf
make hex        # Intel HEX image
make bin        # raw binary image
make size       # flash/SRAM usage, per-section breakdown
make clean      # remove build products
make distclean  # also remove generated headers
```

VS Code tasks (`Build`, `Clean`) and a cortex-debug J-Link launch
configuration are included in `.vscode/`. Adjust `serverpath`,
`ipAddress`, and `device` in `launch.json` for your probe.

## How it works

### Clock bring-up

`SystemInit()` in `src/main.c` overrides the weak platform no-op and runs
before `main()`:

1. Start HEXT (12 MHz crystal), wait for stable.
2. Configure PLL: HEXT undivided × 16 = **192 MHz**, lock while the core
   still runs on HICK 8 MHz. 192 (not the 240 MHz maximum) is chosen so
   USB = PLL/4 = exactly 48 MHz from the crystal, not the trimmed RC.
3. Pre-scale limited domains: APB1/APB2 = 96 MHz (÷2, ceiling 120 MHz),
   ADC = 12 MHz, USB = 48 MHz.
4. Switch SCLK to PLL with **auto-step mode** enabled so the frequency
   ramps in hardware steps, then disable it.

No flash wait-state programming is needed: this family has a
zero-wait-state flash accelerator instead of STM32-style `FLASH->ACR`
latency bits.

`SystemCoreClockUpdate()` (platform layer) re-derives `SystemCoreClock`
from the CRM registers at any time, including the PLL split multiplier
fields and the AHB prescaler encodings.

### Startup

`contrib/platform/src/AT32F403Axx_it.c` is documented for readers new to
Cortex-M bring-up. In short:

- The vector table is generated with the **X-Macro pattern** from
  `AT32F403Axx_handlers.h` (itself generated from the SVD by `irqn`).
- Every handler is a **weak alias** of a default park loop: to handle an
  interrupt, just define `void XXX_IRQHandler(void)` anywhere in your
  code. No registration, no table edits.
- The reset handler enables the FPU, copies `.vector` and `.data` from
  flash to SRAM, zeroes `.bss`, relocates `VTOR` to the RAM vector copy
  (handlers can be redirected at run time), and calls the C runtime.
- Startup uses libc `memcpy`/`memset` deliberately: they are "almost
  pure" (all accesses confined to the caller's buffers), need no
  initialized globals, and newlib's ARM implementations move 64 bytes
  per iteration — faster than hand-rolled byte loops.

### Timebase

`systick_init()` starts a 1 ms SysTick interrupt from `SystemCoreClock`.
`micros()` combines the millisecond counter with the SysTick down-counter
under a short interrupt mask; `delay_us()` burns whole milliseconds on
the tick and the remainder on the cycle counter, accurate to a cycle and
never shorter than requested. See the caveats on `delay()` (WFE-based) in
`include/systick.h`.

## Performance notes

The AT32F403A's "flash" is a **bonded-in QSPI NOR die**, fronted by a
single-line cache (the flash accelerator). A cache miss is a serial QSPI
transaction costing tens to hundreds of core cycles, and the linker
places `.rodata` far after `.text`. Consequences applied in this code:

- Hot paths avoid distant constant data (e.g. the AHB prescaler is
  computed with bit math, not a ROM lookup table).
- Timing-critical ISRs should run from SRAM (the vector table already
  does).

These decisions are documented at their point of use; see also the
session transcript in [PROMPTS.md](./PROMPTS.md).

## License

BSD 3-Clause, see [LICENSE.md](./LICENSE.md). Submodules carry their own
licenses (CMSIS_6: Apache-2.0).