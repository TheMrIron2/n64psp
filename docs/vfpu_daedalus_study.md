# DaedalusX64 PSP VFPU Study

Study date: 2026-06-19.

Repository SHAs:

| Repository | SHA studied |
| --- | --- |
| `TheMrIron2/n64psp` upstream default | `9fe866dbb4b0cec49ab3dbf4ec2c8fe52055e395` |
| `TheMrIron2/n64psp` local implementation checkout | `69e7dff0ac0d3ea435c5d6b1f047cf756049ecf0` |
| `TheMrIron2/sf64-psp` upstream default | `104ea824e865039fca1828916ad2ce32c6b50b0f` |
| `TheMrIron2/sf64-psp` local benchmark checkout | `b72fea678d79d29a9e11eb29483050e2d0e3cc3a` |
| `DaedalusX64/daedalus` | `19736ba2db9bae154f3ae639a0751521c1496ce9` |
| `z2442/oot-PSP` | `c021d1ea3fe8a3c31c6abda23d2197e366848679` |

DaedalusX64 is GPL code and was used as a reference for scheduling, register layout, and algorithm shape only. The `n64psp` implementation is independently written and does not copy Daedalus source.

## Kernels

### `TransformWithColour.S`

Purpose: transform unlit vertices, calculate clip flags, optional fog, optional texture coordinates, and normalized vertex colour.

Layout: input is a 16-byte fiddled N64 vertex record. Output is a 64-byte Daedalus vertex with world position at `0x00`, projected position at `0x10`, colour at `0x20`, texture at `0x30`, and clip flags at `0x38`.

Registers: `M000` holds world, `M100` holds world-projection. `R200` is input/colour scratch, `R201` is world output, `R202` is projected output, `R701` is fog and texture scale, and GPRs hold input, output, end pointer, clip flags, and params.

Strategy: matrices are loaded once per batch. Each vertex is converted from signed 16-bit position fields with `vs2i`/`vi2f`, transformed independently by both matrices, and stored before per-vertex colour/texture work. The loop uses the branch delay slot for output pointer advance.

Dependencies and scheduling: the world and projected transforms both consume the original vector, avoiding a projected-transform dependency on the world result. `vcmp` to `mfvc` has an explicit `vnop`. Colour and texture conversion are scheduled after transform stores while projected data remains resident.

Transferability: high for the two-independent-transform pattern and clip-code idiom; medium for packed vertex conversion because `n64psp` does not yet expose a generic N64 vertex decode API.

### `TransformWithLighting.S`

Purpose: transform lit vertices, compute clip flags, optional fog, texture coordinates, normal transformation/normalization, and directional lighting.

Layout: same 16-byte input and 64-byte output as the colour path.

Registers: `M000` world, `M100` world-projection, `R200` material normal/current normal, `R201` colour accumulator, `R301`/`R302` light normal/colour, `R303` light contribution, `R400`/`S431` scale/alpha, and `R700` ambient.

Strategy: position transforms are still independent. Normal work uses `vtfm3`, `vdot`, `vrsq`, and `vscl`. Light accumulation keeps colour in VFPU registers and uses the MIPS branch delay slot for `vadd`.

Dependencies and scheduling: the normal `vdot -> vrsq -> vscl` chain is unavoidable but surrounded by texture and light memory operations where possible. Clip-code `vcmp -> mfvc` hazards are protected with `vnop` in the older specialized path.

Transferability: high for batch normal transform/normalize and light dot products once `n64psp` has a reusable vertex-lighting API; moderate implementation risk due to register pressure and game-specific lighting policy.

### `TnLVFPU.S`

Purpose: consolidated transform-and-light kernel covering colour, lighting, texture generation, fog, point lights, billboard variants, and clip flags.

Layout: same broad input/output design, with more mode-controlled paths.

Registers: `M000` world, `M100` world-projection or projection depending on mode, `R700`/`R701` params, `R721` texture scale, `R702` original position, `R703` projected position, plus light and scratch rows.

Strategy: this is the clearest mature scheduling reference. It places unrelated loads between `vcmp` and `mfvc`, uses GPR branch delay slots aggressively, keeps matrices resident for the whole batch, avoids per-vertex matrix reloads, and keeps output writes aligned.

Dependencies and scheduling: independent world/projected transforms avoid a `vtfm4 -> vtfm4` chain. Condition-code reads are delayed by a normal/colour load or explicit `vnop`. Light loops interleave `lv.q`, `vdot`, GPR pointer update, `vscl`, branch, and `vadd`.

Transferability: high as a scheduling reference. Direct all-in-one adoption is not suitable for `n64psp` yet because it mixes generic transform, N64 vertex decode, lighting policy, texture generation, and clipping.

### `SysPSP/Math/Math.h`

Purpose: scalar math replacements and small VFPU helpers.

Notable decisions: `sincos` is implemented with `vrot`, vector normalize uses `vdot/vrsq/vscl`, triangle facing avoids perspective divides by multiplying by `Aw*Bw*Cw`, and comments explicitly note that VFPU `fabsf` and `sqrtf` can be slower than FPU equivalents.

Transferability: high for batch sin/cos and normalization APIs, but low for replacing every scalar call blindly. The lesson is to benchmark before moving small scalar operations to VFPU.

### `OSHLE/patch_gu_hle.inl`

Purpose: HLE replacement of N64 GU matrix routines.

Notable decisions: VFPU matrix creation is used for identity, translate, scale, and ortho, but comments warn that unaligned VFPU stores can corrupt registers on PSP-1000. `n64psp` should keep public matrix storage 16-byte aligned and avoid `usv.q` in new generic code unless a PSP-1000-safe path is proven.

Transferability: medium for reusable matrix construction. Risk is moderate because `n64psp` must preserve alignment and host scalar fallbacks.

### `Graphics/ColourValue.cpp`

Purpose: clamp float RGBA to packed colour.

Strategy: load colour, multiply by 255, clamp with `vmax/vmin`, convert with `vf2in`, and store. The file records this as faster than CPU for bulk conversion.

Transferability: high for a future `n64psp` batch colour conversion API. Single-colour calls may not amortize VFPU setup and unaligned loads.

## Specific Answers

1. Daedalus usually transforms the original vector with both the world matrix and a precombined world-projection matrix because it produces both eye/world-space data and clip-space data while avoiding a second transform that consumes the first transform result.
2. This avoids a direct `vtfm4(world) -> vtfm4(projection)` dependency for every vertex, and it avoids reloading or storing the intermediate as the input to projection.
3. Daedalus places unrelated loads, stores, pointer updates, and branch-delay-slot work between producers and consumers. Examples include loading normal/colour data between `vcmp` and `mfvc`, advancing light pointers between `vdot` and `vscl`, and storing transform outputs before texture/colour conversion.
4. Visible hazards include `vcmp` condition-code reads through `mfvc`, reciprocal and reciprocal-square-root consumers (`vrcp`, `vrsq`), and transform/dot consumers when the next instruction immediately needs the result. Older specialized files use explicit `vnop`; `TnLVFPU.S` often uses independent instructions instead.
5. Comparable transform loops are not broadly unrolled. Daedalus mainly wins by matrix residency, independent transforms, and scheduling around latency rather than large unrolled loops.
6. Daedalus intentionally leaves some operations on CPU/FPU when VFPU is slower or awkward: comments call out `sqrtf` and `fabsf`, and clipping colour interpolation in OOT PSP similarly keeps packed RGBA interpolation on the scalar FPU/GPR side.
7. The closest current `n64psp` operation is `n64psp_mat4f_transform_vec4_chain2_batch`; the closest `sf64-psp` call site is display-list vertex processing in `src/psp/gfx/gfx_psp_dl.c`, where modelview and projection-space results are needed for rendering decisions.

## Candidate Ranking

| Candidate | Expected gain | SF64 frequency | Effort | Risk | Reusable | Register pressure | Compatibility | Rank |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Precompose `second * first`, then use independent two-matrix batch as an explicit experiment | Medium at larger counts, possible small-count regression | High in vertex processing | Low | Low/medium due to arithmetic order differences | High | Low | Good; aligned stores only | 1 |
| Two-way interleaving of chained transforms | Medium | High | Medium | Medium, tail handling and scheduling risk | High | Medium | Good if aligned | 2 |
| Batch clip-code generation | Medium/high if SF64 rejects many vertices | Unknown until profiling | Medium | Medium due to condition flags | High | Low | Needs `vcmp/mfvc` hazards handled | 3 |
| Signed-16 position decode to float batch | Medium | High | Medium | Low/medium, layout API needed | High | Low | Avoid `ulv.q` on PSP-1000 | 4 |
| Colour conversion batch | Low/medium | Unknown | Low | Low | High | Low | Useful only when batched | 5 |

## Implemented Experiment

The retained experiment is rank 1: an explicitly named precomposed VFPU benchmark path that composes `second_matrix * first_matrix` once and then calls the existing independent two-matrix VFPU batch with `first_matrix` and the composed matrix. This mirrors Daedalus' matrix residency strategy and avoids the per-vertex `vtfm4` producer/consumer chain in the true chained path.

The public `n64psp_mat4f_transform_vec4_chain2_batch` contract remains strict:

```text
first  = first_matrix * input
second = second_matrix * first
```

The precomposed experiment uses different evaluation order:

```text
combined = second_matrix * first_matrix
first    = first_matrix * input
second   = combined * input
```

These paths are mathematically equivalent under ideal arithmetic, but they can differ in floating-point rounding. The precomposed path is therefore not a drop-in implementation of the public chained API.

Benchmark metadata selector:

```text
N64PSP_VFPU_TRANSFORM_EXPERIMENT=0  true chained VFPU benchmark build
N64PSP_VFPU_TRANSFORM_EXPERIMENT=1  precompose + independent VFPU benchmark-control build
```

Public API impact: none. Scalar fallbacks are unchanged. The selector does not change `n64psp_mat4f_transform_vec4_chain2_batch` arithmetic order; it only labels benchmark/control builds that explicitly measure the experiment.

Expected behavior: the experiment may regress very small counts because one matrix multiply is paid per batch. It could become competitive or faster as count rises if the avoided per-vertex dependency outweighs precomposition cost, but real PSP hardware measurements are required before making any performance claim.

## OOT PSP Architectural Notes

`z2442/oot-PSP` keeps a Fast3D-like frontend in `src/port/psp/gfx/gfx_fast3d.c` and submits through a PSP GU backend in `gfx_scegu.c`, with specialized VFPU helpers such as `gfx_clip_vfpu.s`. This supports the `n64psp` direction of generic helpers below a display-list frontend rather than putting SF64-specific display-list policy into the reusable library.
