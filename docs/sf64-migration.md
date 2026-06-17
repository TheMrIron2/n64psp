# SF64 Migration

1. Common types and result reporting: replace duplicated declarations through a temporary SF64 adapter. Host tests must cover ABI sizes where relevant.
2. Time conversion: compare existing SF64 timing against `n64psp_time_*`; PSP smoke validates monotonic access, hardware still required for pacing.
3. Message queues: initial SF64 PSP hardware integration reached startup and title-screen operation with blocking queue wake-ups across worker threads. Keep high-rate event coalescing in the SF64/platform adapter, stop and join queue users before shutdown, and treat full-game validation as pending.
4. Events and timers: add only after concrete SF64 call sites are audited.
5. Threads and scheduling: route native PSP thread creation through platform adapters; compare lifecycle and priorities.
6. Controller input: keep SF64 mapping code outside the common runtime; platform backend supplies raw polling later.
7. Address and PI helpers: use bridge translation and PI callbacks; old and new reads can be compared on user-supplied data outside n64psp.
8. Audio: add submission interfaces after call-site audit and PSP audio constraints are measured.
9. VI timing: add frame pacing policy without renderer coupling.
10. RSP and graphics-task submission: register an SF64-specific adapter outside the common runtime.
11. Removal: delete superseded SF64 compatibility code only after side-by-side tests and PSP hardware checks where needed.

n64psp must never include SF64 headers. SF64 may include n64psp headers, and temporary adapters belong in SF64 or a separate integration layer.
