# AI tool transcript

This project was reviewed, debugged, and documented in a pair-programming
session with an AI coding assistant. This file records the human prompts
and a summary of what the assistant did in response, so the provenance of
the code and comments is auditable. Commits referenced below are in this
repository's history.

## Session configuration

Recorded from the session's own usage log and tool probes, not from
memory, so the setup can be reproduced:

| Item | Value |
| --- | --- |
| Harness | GitHub Copilot CLI (Copilot SDK) in Visual Studio Code |
| Model | `Qwen Platform/qwen4-max-preview` (custom endpoint, `/responses` API) |
| Reasoning effort | high (session default; per-call log records no override) |
| Subagents | none used in the main thread; all 143 logged API calls ran on the model above |
| Session ID | `3db91aa9-36d1-4beb-bedd-1c2d6c0f6502` |
| Session start | 2026-11-25T08:06:32Z (UTC; prompts below are UTC+8 local time) |
| OS | Windows |
| Compiler | arm-none-eabi-gcc 15.3.1 (Arm GNU Toolchain 15.3.Rel2, arm-15.181) |
| Make | GNU Make 4.4.1 |
| Python | 3.13.14 (runs svdconv/irqn generators) |
| Git | 2.50.1.windows.1 |
| CMSIS submodule | CMSIS_6 v6.4.0 (`af1b110e`) |

Caveats on reproducibility: the model is a hosted preview snapshot and
may be updated or retired by the provider; identical prompts against a
later snapshot are not guaranteed to produce identical output. The
verification steps recorded below (disassembly, exhaustive encoding
checks, clean builds) are deterministic given the toolchain versions
above and can be re-run independently of the model.

---

## 1. Initial review and check-in

> Take a review of this project starter, and check everything into Git
> with appropriate cadence and messages.

The assistant reviewed all sources, the Makefile, linker scripts, and
VS Code configuration; ran a clean rebuild; and fixed leftovers from the
MSPM0L130x template this project was forked from (wrong `@file` headers,
include guard, dead `libusb_stm32` source path, broken `svdFile` path in
`launch.json`, redundant `SystemInit` stub, stale Cortex-M0 comments).
It flagged a CMSIS submodule pin mismatch; the human chose the v6.4.0
release tag. Everything was committed in six layered commits
(`d2921a4`..`dfb328e`).

## 2. SystemCoreClock verification

> Reference the datasheet and see if SystemCoreClock is implemented
> correctly.

The assistant compared the freshly written `SystemCoreClockUpdate()`
against the SVD field layout and Artery's official BSP. Found and fixed
three bugs: an operator-precedence error in the AHB prescaler expression
(`&` vs `+`) that also used wrong shift offsets for the /64../512
encodings; an incomplete HICK 48 MHz condition (needs both
`HICK_TO_SCLK` and `HICKDIV`, not `HICKDIV` alone); and a `HSE_VLAUE`
typo. Committed as `d1558b9`.

## 3. AHB prescaler: table vs expression

> Rethink the AHBDIV. Rather than a 16-byte ROM table, might the
> expression be faster?

The assistant derived `shift = low2 + (bit2 ? 6 : 1)` gated by bit 3,
verified it exhaustively against the table for all 16 encodings in
Python (catching two wrong algebraic variants first), and committed
`cb12a29`.

## 4. Flash accelerator caveats

> I know it is actually longer than the table ROM-wise, but since we do
> have the Flash accelerator - essentially a simple one-line cache, we
> have to be aware.

The assistant disassembled the `-Og` and `-O2` builds, acknowledged the
table would usually hit the accelerator's cached line, and documented
the trade-off (`ba9d87b`).

> Hint: once linker put the whole project together, rodata will be quite
> a distance after text.

Correct observation: the linker script emits `.rodata` after all of
`.text`, so the table would be a guaranteed cold-line fetch that also
evicts the prefetched code line. Comment corrected (`4b4e943`).

> This level of cycle-counting is not seen in modern devs. By the way,
> AFAIK, Artery parts don't actually have on-die Flash, instead they
> have bonded-in QSPI, so the cache miss penalty is huge.

The QSPI NOR architecture detail was added to the rationale: a refill is
a serial transaction, not a few parallel-flash wait states (`96e8e16`).

> My code doing cycle counting in C resulted in my friends scoffing at
> me as being retro. How come?

Discussion only, no code change: source-level cycle counting is
legitimate on in-order Cortex-M parts with deterministic buses and a
one-line cache over serial flash, and misapplied on out-of-order
application processors where the hardware hides timing.

## 5. Startup code annotations

> Annotate this file and the accompanying .h file so people without
> knowledge to the Cortex-M bring-up or X-Macro can understand. I had
> multiple FAEs questioning me over this template file.

The assistant annotated `AT32F403Axx_it.c` / `.h`: hardware vector fetch,
the +16 system-exception offset, weak-alias handler overriding, linker
LMA/VMA symbols, FPU enable, and the multi-pass X-Macro expansion. A
first draft broke the build because comments inside macro-expansion
zones spelled expandable tokens (the preprocessor expands macros even
in comments there); fixed and noted as a trap for future editors
(`10f7fdf`).

> And add an explanation why the almost-pure memcpy and memset functions
> can be used here (instead of the often manually rolled for loops.)
> You may want to check compiler optimizations here as with FPU enabled,
> memcpy and memset may become some highly efficient FPU-based stuff.

The assistant disassembled the linked newlib `memcpy`/`memset`: pure
register loops, word-aligned, 64 bytes/iteration (16 unrolled LDR/STR
pairs) and STRD-based 16-byte clears. Safe before `.data`/`.bss` init
because they touch nothing outside their arguments. The FPU hypothesis
was checked and rejected for this build: newlib's memcpy is
integer-only, and Cortex-M4 has no NEON (`9057557`).

> A pure function can't write pointers but memset and memcpy do, but all
> its pointer access is well restrained and defined so that is why I
> call them "almost pure."

Terminology adopted: not strictly pure (writes through pointers), but
all effects confined to caller-passed buffers (`cbb3328`).

> And how I wrote it actually made LTO a good thing to do: while the
> symbols are not compile-time constants, they are constants at link
> time, and enabling LTO will result in those calls eventually be
> replaced by compiler unrolls.

The assistant tested this empirically at `-Og -flto`, `-O2 -flto`, and
`-flto-partition=none`, plus a minimal freestanding case: the calls
survive, because GCC's LTO phase runs before the linker assigns final
addresses, so the span expressions are still relocations at LTO time.
Folding would need a post-link optimizer. Theory and measurement both
documented (`f9b5a66`).

## 6. SystemInit review

> Now we have a working build. Figure out if SystemInit needs further
> annotation, and pace commits appropriately with appropriate messages.

The human had written a 192 MHz clock bring-up in `main.c`. The
assistant verified every register write against the SVD and Artery BSP
(PLL multiplier encoding, USB/ADC/APB divider encodings, ordering,
SCLKSTS polling, no FLASH->ACR on this family), corrected one comment
(ADC divides APB2, not SCLK), and added the missing **auto-step mode**
bracket around the SCLK switch that every vendor example uses.
Annotated the pre-`.data` contract (no `SystemCoreClockUpdate()` call
here) and the 192 MHz-for-exact-48 MHz-USB rationale. The human's
`SystemCoreClock = 8000000` initializer was committed separately
(`4b294d1`, `ebbff57`).

## 7. Documentation

> Cook a good README for me, and provide an AI tool transcript in
> PROMPTS.md.

This README and transcript.

> Edit the PROMPTS.md front matter to include not only the harness but
> also the model's own name and configuration. Since that file is a
> provenance log, make it reproducible.

The assistant pulled the model identity and call counts from the
session's own usage log (rather than asserting them from memory), probed
the toolchain versions, and added the Session configuration table above,
with a caveat that hosted preview models are not pinned snapshots
(`ec258da`).

## 8. Blinker

> Now I have the blinker running. Annotate the code appropriately,
> update README and PROMPTS, commit and I'll prep the release.

The human added a PB13 LED blinker to `main()` and confirmed it running
on hardware. The assistant verified the GPIO configuration against the
SVD (F1-style CFGHR fields; `GPIOA_*` macro names apply to GPIOB via
SVD `derivedFrom`), annotated the bus-clock prerequisite, the mode/
function encodings, and the RMW-toggle concurrency caveat (atomic
`SCR`/`CLR` alternative), and updated the README with a Blinker section.