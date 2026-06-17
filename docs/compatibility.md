# Compatibility

## Runtime APIs

| API | Status | Notes |
| --- | ------ | ----- |
| Runtime registration/init/shutdown | Implemented | Host tested and PSP cross-compiled. |
| `osCreateMesgQueue` | Implemented | Caller storage, libultra/SF64-compatible public field names, side metadata, host tests, PSP smoke coverage. |
| `osSendMesg` | Implemented | Blocking and nonblocking tested on host and in PSP smoke. |
| `osJamMesg` | Implemented | Host tested. |
| `osRecvMesg` | Implemented | Blocking and nonblocking tested on host and in PSP smoke. |
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

## Renderer/Task Services

Trace backend is implemented. It records submitted task metadata and returns `N64PSP_ERROR_UNSUPPORTED`.
