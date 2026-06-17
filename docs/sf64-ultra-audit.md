# SF64 Ultra Audit

Inspected repository: `TheMrIron2/sf64-psp`, default branch `master`, commit `71af57cdd47c4f547d4593e00c915041c32ff73b`. The temporary inspection clone was outside this repository.

This audit focuses on the PSP port surface, especially `src/psp/ultra_reimpl.c`, `src/psp/platform.c`, and representative call sites in game/audio/system code. Original `src/libultra/**` files and `include/PR/**` declarations are present too, but the PSP replacement layer is the relevant migration target.

## PSP Runtime Work In `PspPlatform_*`

| Function | Location | Libultra-style responsibility | Intended n64psp owner |
| -------- | -------- | ----------------------------- | --------------------- |
| `PspPlatform_SetEventMesg` | `src/psp/platform.c:299` | Stores OS event queue/message registrations used by SP/DP/VI style completion events. | Runtime events, then platform adapter |
| `PspPlatform_SetViEvent` | `src/psp/platform.c:306` | Stores VI retrace queue/message and retrace count. | Runtime VI timing |
| `PspPlatform_PostViEvent` | `src/psp/platform.c:312` | Posts VI messages with `osSendMesg` and drives per-frame diagnostics. | Runtime VI timing plus platform app hook |
| `PspPlatform_PollInput` | `src/psp/platform.c:320` | Fills `OSContPad` through PSP input and can request exit. | Platform input backend plus SF64 adapter |
| `PspPlatform_RunGfxTask` | `src/psp/platform.c:327` | Dispatches graphics task and posts SP/DP event messages. | Renderer/task backend |
| `PspPlatform_RunAudioTask` | `src/psp/platform.c:345` | Handles audio task completion and posts SP event message. | Renderer/task backend or audio backend |
| `PspPlatform_LogLine`, `PspPlatform_LogFrame`, `PspPlatform_LogValue` | `src/psp/platform.c:208`, `238`, `379` | Diagnostics used across PSP audio, renderer, and system code. | Platform logging |
| `PspPlatform_Init`, `PspPlatform_RequestExit` | `src/psp/platform.c:263`, `295` | PSP app lifecycle and audio output setup. | PSP application/backend, not neutral runtime |

## SF64 PSP `os*` Inventory

| Symbol | SF64 definition | Important call sites | Current SF64 PSP behavior | n64psp status | Direct replacement? | Adapter or blocker | PSP hardware validation |
| ------ | --------------- | -------------------- | ------------------------- | ------------- | ------------------- | ------------------ | ----------------------- |
| `osCreateMesgQueue` | `src/psp/ultra_reimpl.c:124` | `src/audio/audio_load.c:1041`, `src/audio/audio_thread.c:254`, `src/sys/sys_timer.c`, many game queues | Initializes caller buffer; no native wait objects; `validCount` is current count, `msgCount` capacity. | Implemented | Yes. | Existing SF64 headers define `OSThread* mtqueue/fullqueue`; n64psp uses opaque pointer fields for ABI shape. Reinitialisation requires a quiescent queue. | Initial PSP hardware validation through startup/title-screen operation. Blocking wake-up worked across PSP worker threads. |
| `osSendMesg` | `src/psp/ultra_reimpl.c:133` | Audio load/thread queues, VI/SP/DP event posting, save requests | Uses interrupt lock plus `sceKernelDelayThread(1000)` polling for blocking sends. | Implemented | Yes. | n64psp preserves libultra-visible `0` on success and `-1` on failure. Detailed native results stay internal or behind separate native APIs. | Initial PSP hardware validation through startup/title-screen operation; producer wake-up and no busy wait observed. |
| `osJamMesg` | `src/psp/ultra_reimpl.c:159` | `src/libultra/io/epidma.c:55` and PI command paths | Front insertion but shifts messages after moving `first`; uses polling for blocking. | Implemented | Yes. | n64psp inserts at the logical front without shifting the whole queue buffer and preserves `0`/`-1` return semantics. | Host wraparound coverage; full-game PSP validation pending. |
| `osSendMesgNoBlock` | `src/psp/ultra_reimpl.c:191` | PSP helper-only wrapper | Calls `osSendMesg(..., OS_MESG_NOBLOCK)`. | Not implemented | Trivial adapter. | Add inline/source adapter in SF64 or n64psp compatibility layer if call sites remain. | Not special. |
| `osRecvMesg` | `src/psp/ultra_reimpl.c:198` | Audio DMA queues, SI/controller waits, VI/task queues, macros in `include/sf64thread.h:191` | Uses interrupt lock plus `sceKernelDelayThread(1000)` polling for blocking receives. | Implemented | Yes. | n64psp avoids busy-waiting and preserves libultra-visible `0`/`-1` return semantics. Runtime shutdown requires all queue users/waiters to stop first. | Initial PSP hardware validation through startup/title-screen operation; consumer wake-up observed. |
| `osSetTimer` | `src/psp/ultra_reimpl.c:258` | `src/sys/sys_timer.c:26`, `48`, controller init timer paths | Allocates from fixed timer pool, creates one PSP thread per timer, posts message on expiry/repeat. | Planned | No | Requires runtime timer API and PSP timer/thread policy. | Timer accuracy, cancellation, repeated timers. |
| `osStopTimer` | `src/psp/ultra_reimpl.c:286` | Timer users | Terminates/deletes PSP timer thread. | Planned | No | Needs timer lifecycle and safe cancellation. | Timer thread cleanup. |
| `osGetTime` | `src/psp/ultra_reimpl.c:303` | Audio profiling, game timing, mods | Converts `sceRtcGetCurrentTick` using `sceRtcGetTickResolution`. | Partially implemented via `n64psp_time_*` | Adapter needed | Need `osGetTime` wrapper returning `OSTime`. | Timing drift and frame pacing. |
| `osSetTime` | `src/psp/ultra_reimpl.c:310` | Declarations; no important PSP call site found | No-op. | Not implemented | No | Decide whether to support runtime time offset. | Low priority. |
| `osGetCount` | `src/psp/ultra_reimpl.c:314` | Audio random seed, VI timing, profiler | Returns low 32 bits of `osGetTime`. | Partially implemented via tick conversion | Adapter needed | Add `osGetCount` wrapper once `OSTime` policy is fixed. | Wrap behavior. |
| `osCreateThread` | `src/psp/ultra_reimpl.c:340` | VI manager, boot/main/game threads, audio manager | Registers an `OSThread` in a fixed PSP thread pool. | Platform callback only | No | Need libultra-shaped thread object and scheduler policy. | Stack/priority mapping. |
| `osStartThread` | `src/psp/ultra_reimpl.c:360` | Game/audio/VI thread startup | Creates and starts PSP thread with mapped priority and fixed stack. | Platform callback only | No | Needs libultra thread wrapper. | Thread lifecycle. |
| `osStopThread` | `src/psp/ultra_reimpl.c:383` | rmon/task paths, declarations | Marks stopped only; does not terminate PSP thread here. | Not implemented | No | Needs semantics decision. | Thread cancellation safety. |
| `osDestroyThread` | `src/psp/ultra_reimpl.c:387`; `src/psp/platform.c` has no equivalent except helper use | Thread cleanup paths | Terminates/deletes matching PSP thread. | Platform callback destroy only | No | Needs libultra thread wrapper. | Resource cleanup. |
| `osYieldThread` | `src/psp/ultra_reimpl.c:401` | Device-manager waits and thread helpers | `sceKernelDelayThread(0)`. | Platform sleep/yield available indirectly | Adapter needed | Add runtime/platform yield wrapper. | Low. |
| `osGetThreadId` | `src/psp/ultra_reimpl.c:405` | Declarations and possible debug paths | Returns stored N64 thread id. | Not implemented | No | Needs `OSThread`. | Low. |
| `osSetThreadPri` | `src/psp/ultra_reimpl.c:409` | VI manager priority juggling | Updates stored priority and PSP thread priority. | Not implemented | No | Needs `OSThread` and priority mapping. | Priority behavior. |
| `osGetThreadPri` | `src/psp/ultra_reimpl.c:427` | VI manager | Returns stored priority or 0. | Not implemented | No | Needs `OSThread`. | Low. |
| `osWritebackDCacheAll` | `src/psp/ultra_reimpl.c:431` | Audio heap, blur/render paths | Calls `sceKernelDcacheWritebackAll`. | Planned platform cache service | No | Add cache callbacks only where required. | Hardware cache behavior. |
| `osWritebackDCache` | `src/psp/ultra_reimpl.c:435` | RSP task load and DMA paths | Calls `sceKernelDcacheWritebackRange`. | Planned | No | Cache API needed. | Hardware cache behavior. |
| `osInvalDCache` | `src/psp/ultra_reimpl.c:439` | Audio DMA/sample paths | Calls `sceKernelDcacheInvalidateRange`. | Planned | No | Cache API needed. | Hardware cache behavior. |
| `osInvalICache` | `src/psp/ultra_reimpl.c:443` | Initialization paths | Invalidates all I-cache, ignores range. | Planned | No | Cache API needed. | Hardware cache behavior. |
| `osAiSetFrequency` | `src/psp/ultra_reimpl.c:449` | `src/audio/audio_heap.c:697` | Returns requested frequency. | Planned audio backend | No | Need audio submission/resampling policy. | Audio pitch/rate. |
| `osAiGetLength` | `src/psp/ultra_reimpl.c:589` | `src/audio/audio_thread.c:64` | Returns queued PSP audio bytes. | Planned audio backend | No | Need audio output API. | Audio latency. |
| `osAiSetNextBuffer` | `src/psp/ultra_reimpl.c:593` | `src/audio/audio_thread.c:67` | Submits PCM to PSP audio output. | Planned audio backend | No | Need audio ownership/copy policy. | Audio underrun/overrun. |
| `osSetEventMesg` | `src/psp/ultra_reimpl.c:453` | VI manager, rmon, task events | Forwards to `PspPlatform_SetEventMesg`. | Planned | No | Runtime event registry needed. | Event ordering. |
| `osViSetEvent` | `src/psp/ultra_reimpl.c:457` | VI manager | Forwards to PSP VI event storage. | Planned | No | Runtime VI timing/event API needed. | Frame pacing. |
| `osInitialize` | `src/psp/ultra_reimpl.c:461` | Boot/init | Sets `osTvType = OS_TV_NTSC`. | Not implemented | Adapter needed | Global libultra variables not yet exposed. | Low. |
| `osEepromProbe`, `osEepromRead`, `osEepromWrite`, `osEepromLongRead`, `osEepromLongWrite` | `src/psp/ultra_reimpl.c:465`-`485` | Save/controller code | Probe reports present; reads zero; writes ignored. | Planned save backend | No | Need save-medium adapter; current SF64 behavior is not a faithful save implementation. | Save persistence. |
| `osContStartReadData`, `osContGetReadData`, `osContInit`, `osContReset`, `osContStartQuery`, `osContGetQuery`, `osContSetCh` | `src/psp/ultra_reimpl.c:493`-`538` | Input loops and controller init | Sends queue messages and maps PSP input to controller 1; controllers 2-4 disconnected. | Planned input backend | No | Need platform input callback plus libultra adapter. | Real controller mapping. |
| `osCreateViManager`, `osViSetMode`, `osViSetSpecialFeatures`, `osViSwapBuffer`, `osViBlack`, `osViRepeatLine`, `osViGetCurrentMode`, `osViGetCurrentLine` | `src/psp/ultra_reimpl.c:543`-`571` | VI/render/game reset paths | Mostly no-op/status stubs. | Planned VI/display backend | No | Need renderer/VI split and SF64 adapter. | Visual timing/display behavior. |
| `osCreatePiManager` | `src/psp/ultra_reimpl.c:575` | PI manager setup | Creates command queue only. | Bridge/platform planned | No | Needs PI manager or adapter over bounded ROM reads. | DMA ordering. |
| `osCartRomInit`, `osDriveRomInit` | `src/psp/ultra_reimpl.c:580`, `585` | Audio load and ROM paths | Returns static handle. | Planned bridge adapter | No | Needs handle type and ROM region registration. | Low. |
| `osPiStartDma`, `osEPiStartDma` | `src/psp/ultra_reimpl.c:597`, `614` | Audio/sample DMA, device manager | For `OS_READ`, copies from `(void*)devAddr` to `dramAddr`, then sends ret message. | Bridge has bounded ROM read helper only | No | Pointer/address conflation blocks direct replacement; needs bounded PI/ROM backend adapter. | ROM/DMA correctness. |
| `osPiGetStatus`, `osPiGetDeviceType`, `osPiWriteIo`, `osPiReadIo` | `src/psp/ultra_reimpl.c:619`-`633` | Init/device paths | Mostly zero/no-op. | Planned bridge/platform | No | Need exact required behavior per call site. | Low. |
| `osVirtualToPhysical` | `src/psp/ultra_reimpl.c:641` | RSP task/DMA register setup | Casts pointer to `u32`. | Bridge rejects arbitrary pointer/address conflation | No | Needs source-port adapter if game still expects identity mapping. | Address assumptions. |
| `osDpSetStatus`, `osDpGetStatus` | `src/psp/ultra_reimpl.c:645`, `649` | RDP/task paths | No-op / zero. | Planned renderer/task backend | No | Need renderer-specific status model or adapter. | Renderer behavior. |
| `osSpTaskLoad`, `osSpTaskStartGo`, `osSpTaskYield`, `osSpTaskYielded` | `src/psp/ultra_reimpl.c:653`-`676` | Audio/gfx task submission | Stores task, dispatches audio vs gfx to `PspPlatform_Run*Task`, yield is no-op. | Trace task submission implemented only | No | Need real task classifier and SF64 renderer/audio adapters. | Graphics/audio task completion. |
| `osMotorInit`, `osMotorStart`, `osMotorStop` | `src/psp/platform.c:410`-`422` | Rumble/controller paths | Init returns `-1`; start/stop return success without work. | Out of current scope | No | Keep SF64-side adapter until rumble policy exists. | Hardware feature availability. |
| `osSyncPrintf` | `src/mods/isviewer.c:71` | Diagnostics | Project-specific printf/log shim. | Platform logging exists | Adapter likely | Route to `n64psp_log` or game logger. | Low. |
| `osReadHost` | `src/libultra/host/readhost.c:15` | RDB/host tooling | Original-style host read path; not PSP runtime core. | Out of current scope | No | Exclude unless SF64 still links it in PSP build. | Low. |
| `osLeoDiskInit` | `src/libultra/io/leodiskinit.c:10` | 64DD paths | Original libultra code, not PSP migration target. | Out of scope | No | Keep outside n64psp unless a port needs 64DD. | Low. |

## Calls Not Backed By PSP Reimplementation

The broader tree contains declarations or original libultra files for many additional `os*` names, including flash, PFS, RDB, region allocator, profile, voice, TLB, and VI mode symbols. They appear in `include/PR/**`, `src/libultra/**`, linker scripts, or non-PSP original source. They are not current n64psp bootstrap targets unless SF64's PSP build links or calls them through `src/psp/ultra_reimpl.c` or game-side PSP paths.

## Targeted z2442 Comparison

For SF64-relevant symbols, z2442 ports show the same high-demand cluster: message queues, time/count, threads, audio queues, and PI/DMA-style reads.

- `z2442/perfect_dark-PSP` commit `0871c907aea105cd2e7002219d047c733011f668`, `port/src/libultra.c:89` defines `osCreateMesgQueue` with `validCount = 0`, `msgCount = count`; its `osSendMesg`/`osRecvMesg` at `102` and `107` are narrow stubs in the inspected branch.
- `z2442/sm64-port` commit `8c219c77a94ed458958cbe661db7e1f537dc8b26` has libultra queue implementations under `lib/src/osCreateMesgQueue.c`, `osSendMesg.c`, `osJamMesg.c`, and `osRecvMesg.c`, and many source call sites in audio/game code.
- `z2442/oot-PSP` commit `c021d1ea3fe8a3c31c6abda23d2197e366848679` has heavy queue use in audio/DMA/thread managers, for example `src/audio/internal/thread.c` and `src/boot/z_std_dma.c`.

The useful cross-port conclusion is narrow: queues and time/count wrappers are common early migration surfaces; SF64's busy-waiting PSP queue implementation was a correctness/performance shortcut, and n64psp queue primitives are now direct replacements for the queue portion when integrated with the correct SF64 type/layout adapter.

## Directly Replaceable Today

`osCreateMesgQueue`, `osSendMesg`, `osJamMesg`, and `osRecvMesg` are direct replacements today for the SF64 queue behavior covered by initial PSP hardware validation. The n64psp compatibility entry points already preserve SF64/libultra-visible `0` and `-1` return values; detailed `n64psp_result` values must remain internal or behind separate native APIs.

High-frequency event coalescing, including VI retrace coalescing, is an SF64/platform-adapter responsibility rather than generic queue behavior. Full-game and long-duration PSP validation remain pending.

Everything else needs either a new n64psp runtime subsystem, a bridge/platform callback, or an SF64-specific integration adapter.
