# n64psp

`n64psp` is an early reusable C runtime and adaptation library for porting Nintendo 64 decompilation and static-recompilation projects to the Sony PSP.

## Architecture

The project has four layers:

- Runtime: portable scheduling, queues, time, diagnostics, and future libultra-shaped services.
- Bridge: optional N64Recomp/RDRAM/PI/overlay helpers.
- Platform backend: host and PSP implementations of time, logging, semaphores, mutexes, threads, and future I/O.
- Renderer/task backend: RSP/graphics task recognition and dispatch.

N64ModernRuntime informed the split between a reusable runtime and a recompilation bridge, but this project deliberately uses C ABI structures, function tables, explicit ownership, and bounded allocation instead of desktop C++ abstractions.

## Tests

Host tests do not require ROMs or proprietary data. PSP-specific behavior is compiled and tested by the PSP smoke app.