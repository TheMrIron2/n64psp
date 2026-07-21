# Compatibility

## Runtime APIs

| API | Status | Notes |
| --- | ------ | ----- |
| Runtime registration/init/shutdown | Implemented | Host tested and PSP cross-compiled. Shutdown returns `N64PSP_ERROR_BUSY` without tearing down the runtime if queue users or waiters remain active. |
| `osCreateMesgQueue` | Implemented | Caller storage, libultra/SF64-compatible public field names, side metadata, host tests, PSP smoke coverage. Reinitialisation requires a quiescent queue. |
| `osSendMesg` | Implemented | Blocking and nonblocking tested on host and in PSP smoke. Preserves libultra-visible `0`/`-1` returns. |
| `osJamMesg` | Implemented | Host tested, including front insertion after wraparound. Preserves libultra-visible `0`/`-1` returns. |
| `osRecvMesg` | Implemented | Blocking and nonblocking tested on host and in PSP smoke. Preserves libultra-visible `0`/`-1` returns. |
| Time conversion | Implemented | Host tested; PSP smoke prints monotonic/tick sanity. |

## Bridge APIs

| API | Status | Notes |
| --- | ------ | ----- |
| RDRAM registration/translation | Implemented | Bounds checked; host tested; PSP smoke compiled/exercised. |
| Big-endian loads/stores | Implemented | 16/32/64-bit helpers. |
| Bounded ROM reads | Implemented | Explicit `n64psp_rom` helper with bounds checks. |
| PI read callback | Partially implemented | Callback plumbing exists; callers must provide a bounded backend. |

## Platform Services

Host backend is implemented with pthread-private internals. PSP backend is implemented with PSPSDK semaphores, threads, RTC ticks, and debug/stdout logging.

The queue subsystem has also completed initial integration validation in
`TheMrIron2/sf64-psp` on real PSP hardware through startup and title-screen
operation. Blocking queue wake-ups worked across SF64's PSP worker threads and
performance was comparable to SF64's previous polling queue implementation.
This is not a complete playthrough or exhaustive edge-case validation.

Additional physical PSP diagnostics showed the standalone optimized queue hot
path around 1.2-1.4 million uncontended operations per second and the
capacity-one two-thread blocking ping-pong around 115,000 queue operations per
second. SF64 title-screen aggregate counters showed matched block/wake/retry
traffic, no spurious wake-ups, and no evidence of stale notification tokens.
PPSSPP currently shows a much larger slowdown with the same queue counters, so
that remains an emulator-specific scheduling/timing discrepancy pending more
evidence.

Applications must stop and join all queue-using threads before final runtime
shutdown. High-rate event coalescing is an application/platform-adapter policy,
not part of the generic message queue implementation.

## Renderer/Task Services

Trace backend is implemented. It records submitted task metadata and returns `N64PSP_ERROR_UNSUPPORTED`.

## Transform and Lighting

| API | Status | Notes |
| --- | ------ | ----- |
| Packed vertex transform | Experimental | Scalar tested and PSP hardware validated in SF64 with a dependent modelview-projection chain |
| Packed vertex transform with lighting | Experimental | PSP hardware validated in SF64 without position or normal staging |
