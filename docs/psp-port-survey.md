# PSP Port Survey

This bootstrap performed repository-page and branch-head inspection only, not a full code clone or symbol audit. The table below separates observations from pending research.

| Subsystem | SM64 | Perfect Dark | OOT | Shared pattern | Candidate n64psp design |
| --------- | ---- | ------------ | --- | -------------- | ----------------------- |
| Build system | `master` branch verified, pending code search | `PSP-Port` page lists `cmake`, `dist`, `port`, `src` | `main` branch verified, pending code search | PSP-specific build integration exists in practical ports | Use installed PSPSDK tools through the repository Makefile |
| Message queues | Unknown pending inspection | Unknown pending inspection | Unknown pending inspection | Not established | Runtime owns queue semantics with platform semaphores |
| Threads | Unknown pending inspection | Unknown pending inspection | Unknown pending inspection | Not established | Platform backend owns native threads |
| Timers | Unknown pending inspection | Unknown pending inspection | Unknown pending inspection | Not established | Runtime exposes tick policy; platform supplies monotonic time |
| RSP/graphics | Unknown pending inspection | Unknown pending inspection | Unknown pending inspection | Not established | Renderer/task backend owns task recognition |
| PSP lifecycle | Unknown pending inspection | Unknown pending inspection | Unknown pending inspection | PSP apps need callbacks | PSP smoke installs exit callback |

| Symbol | SM64 | Perfect Dark | OOT | Required semantics | Proposed n64psp owner |
| ------ | ---- | ------------ | --- | ------------------ | --------------------- |
| `OSMesgQueue` | Unknown pending inspection | Unknown pending inspection | Unknown pending inspection | Caller-supplied ring buffer, blocking/nonblocking send/receive | Runtime |
| `osSendMesg` | Unknown pending inspection | Unknown pending inspection | Unknown pending inspection | FIFO send, full handling, producer wake-up | Runtime |
| `osJamMesg` | Unknown pending inspection | Unknown pending inspection | Unknown pending inspection | Front insertion | Runtime |
| `osRecvMesg` | Unknown pending inspection | Unknown pending inspection | Unknown pending inspection | Empty handling, consumer wake-up | Runtime |
| `OSTask` | Unknown pending inspection | Unknown pending inspection | Unknown pending inspection | Validate and route to task backend | Renderer/task backend |

## Pending Searches

Run code search or temporary clones outside this repository for `os*`, `OSMesgQueue`, `OSMesg`, `OSThread`, `OSTimer`, `OSTask`, `OSContPad`, `sceKernelCreateThread`, `sceKernelCreateSema`, `sceCtrl`, `sceAudio`, `sceDisplay`, `sceKernelDcache`, PSPGL/GU, PRX, Media Engine, empty stubs, always-success functions, and busy loops.
