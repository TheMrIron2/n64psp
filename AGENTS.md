# AGENTS

Read `README.md` and `docs/architecture.md` before changing code.

Preserve the runtime, bridge, platform backend, and renderer/task backend boundaries. N64ModernRuntime is the primary architectural reference, z2442's OOT/Perfect Dark/SM64 PSP ports are primary practical PSP references, SF64 PSP is the first migration target, and libultraship is secondary only.

Never add copyrighted game material, ROM data, proprietary SDK files, or game-specific logic to the common runtime. Do not claim PSP hardware or emulator testing unless it actually happened.

Use `make` as the canonical PSP-first build. Run `make`, `make inspect-psp`, `make test`, and `make smoke-host` before finishing relevant changes. Add host tests for portable behavior and PSP smoke coverage for PSP-specific behavior. Keep PSPSDK includes and handles out of neutral public headers. Check PSPSDK return values.

Prefer correct scalar implementations before VFPU, GU, Media Engine, or asynchronous fast paths. Benchmark before adding optimized paths.

Update `docs/compatibility.md` whenever APIs change. Avoid silent stubs and false success results.
