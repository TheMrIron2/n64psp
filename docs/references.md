# References

## Accessible During Bootstrap

| Repository | Branch observed | Evidence |
| ---------- | --------------- | -------- |
| `N64Recomp/N64ModernRuntime` | `main` at `ae1ffbb909d9f93c88c41830deb539f7feef5ed2` | GitHub page accessible; repository lists `ultramodern`, `librecomp`, `thirdparty`, and README text describing runtime/bridge responsibilities. |
| `z2442/perfect_dark-PSP` | `PSP-Port` at `0871c907aea105cd2e7002219d047c733011f668` | GitHub page accessible; repository is a fork of the Perfect Dark PC port and lists PSP-oriented folders including `cmake`, `dist`, `include`, `port`, and `src`. |
| `z2442/sm64-port` | `master` at `8c219c77a94ed458958cbe661db7e1f537dc8b26` | GitHub page accessible; code-level symbol inspection remains pending. |
| `z2442/oot-PSP` | `main` at `c021d1ea3fe8a3c31c6abda23d2197e366848679` | Branch head verified with `git ls-remote`; browser page inspection was unreliable, so code-level symbol inspection remains pending. |
| `TheMrIron2/sf64-psp` | `master` at `71af57cdd47c4f547d4593e00c915041c32ff73b` | GitHub page accessible; repository describes a PSP port of Star Fox 64 based on the decompilation. |

## N64ModernRuntime Mapping

Observed from the repository README: `ultramodern` covers threads, controllers, audio, message queues, timers, RSP task handling, and VI timing, while `librecomp` bridges N64Recomp-generated code, PI DMA/ROM reads, overlays, and save media. `n64psp` adopts those responsibility boundaries conceptually, but rejects desktop assumptions such as C++20, exceptions, RTTI, virtual renderer classes, desktop window dependencies, and unbounded allocation in neutral runtime surfaces.

No implementation bodies were copied.
