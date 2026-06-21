# n64psp

`n64psp` is a reusable C library for porting N64 projects to the PSP.

## Capabilities

The capability of n64psp is currently under constant expansion, but its goals are to become:

- A high-quality implementation of VFPU work demonstrated by DaedalusX64
- A smooth layer between N64 OS and PSP calls
- Able to produce optimised code for any common, reused functionality between N64 ports

## Tests

Tests do not require ROMs or proprietary data. PSP-specific behavior is compiled and tested by the PSP smoke app.

Currently, it is implemented into just [sf64-psp](https://github.com/TheMrIron2/sf64-psp) during development, but it is expected to become part of most or all PSP-N64 ports to bring up the wider quality of code and performance.
