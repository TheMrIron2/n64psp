# Architecture

`n64psp` follows the responsibility boundary shown by N64ModernRuntime: reusable runtime behavior is separate from generated-code bridge behavior, and graphics/task handling is supplied by a registered backend. N64ModernRuntime's repository page identifies `ultramodern` as the libultra-like runtime and `librecomp` as the N64Recomp bridge layer; `n64psp` keeps the same conceptual division while avoiding implementation copying.

## Runtime

The runtime owns portable services such as message queues, time conversion, diagnostics, and future events, timers, controller state, audio submission, and scheduling policy. It must not know about SF64, ROM extraction layouts, PSPGL/GU, SDL, or generated N64Recomp function names.

## Bridge

The bridge owns RDRAM translation, PI reads, endianness helpers, and future N64Recomp ABI shims. Source ports can link the runtime without the bridge.

## Platform Backend

Platform callbacks provide monotonic time, sleeping, logging, fatal diagnostics, semaphores, mutexes, threads, and PI reads. Neutral headers expose opaque handles only; pthread and PSPSDK types stay in backend source files.

## Renderer/Task Backend

Task submission validates `OSTask`-shaped inputs and calls a registered backend. The trace backend records diagnostics and returns `N64PSP_ERROR_UNSUPPORTED`; unknown tasks are not reported as success.

## PSP Policy

The PSP backend uses PSPSDK kernel semaphores and threads with explicit stack sizes and priorities. Optimized VFPU/GU/asynchronous paths are future work and require scalar fallbacks plus tests or benchmarks.

