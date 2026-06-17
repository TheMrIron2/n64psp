# Roadmap

1. Repository, host tests and PSP build foundation: current bootstrap.
2. Message queues, events, timers and threads: expand beyond queues into timer/event semantics.
3. Input, audio and VI timing: add integration-driven APIs after real port call-site audits.
4. RSP-task and graphics path: define concrete task classifiers and renderer adapters.
5. PSP performance work: benchmark scalar paths before VFPU, GU, or asynchronous variants.
6. SF64 integration: replace private compatibility code incrementally through adapters.
7. Broader source-port compatibility: add APIs only when backed by call sites and tests.
8. Optional N64Recomp bridge expansion: add generated-code ABI shims without making direct ports link them.

Media Engine work is experimental future work and requires explicit ownership, measurement, and a main-CPU fallback.

