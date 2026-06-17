# Porting Guide

Link `n64psp_runtime` and a platform backend. Direct source ports may skip `n64psp_bridge`; recompilation-based ports can add it for RDRAM and PI helpers.

Register platform callbacks, then renderer/task callbacks, then call `n64psp_runtime_init`. Register RDRAM with `n64psp_rdram_register` and route ROM/PI reads through the platform `pi_read` callback.

Game-specific controller mapping, save routing, renderer behavior, and temporary migration adapters belong in the game port or a distinct integration layer, not in the common runtime.

PSP applications should build through the repository Makefile and installed PSPSDK tools, link the PSP backend, install normal PSP exit callbacks, and keep ROM/game data user-supplied outside this repository.
