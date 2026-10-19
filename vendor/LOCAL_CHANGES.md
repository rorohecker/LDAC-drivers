# LDAC encoder provenance and local change

Upstream: https://android.googlesource.com/platform/external/libldac/
Revision: eeee1a3f5f8df1282e3a6d297085885fd886737b
Sony's LICENSE and NOTICE are preserved in libldac/.

Five local source adaptations (paths below are under `libldac/`):

- `src/bitalloc_sub_ldac.c`: omit GCC's unused-function attribute under MSVC.
- `src/struct_ldac.h`: define `UNUSED_ATTR` as empty under MSVC.
- `src/ldacBT_internal.h`: omit the unavailable, unused `unistd.h` under MSVC.
- `src/pack_ldac.c`: cast idata to unsigned int before
left-shifting its bit pattern. UBSan found a shift of a negative signed integer
in the upstream implementation during real audio encoding. The masked result
uses the low 24 bits, and shifting the unsigned representation gives defined
C semantics. The source comment identifies the local modification.
- `src/setpcm_ldac.c`: assemble packed 24-bit PCM with unsigned shifts, then
  explicitly sign-extend and scale it. UBSan found signed shifts in the
  upstream implementation when the new 24-bit offline encoder test ran.

The 2026-09-19 audit compared all 32 vendored files with the pinned upstream
revision after normalizing line endings: 28 unchanged, four modified. The
`setpcm_ldac.c` fix above was added after that audit, bringing the current
source adaptation count to five.
A separately rebuilt upstream encoder with only the three MSVC build adaptations
produced byte-identical output for 36 cases (four input corpora, three quality
modes, three MTUs). This is bounded regression evidence, not Sony certification.
