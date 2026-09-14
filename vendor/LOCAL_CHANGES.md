# LDAC encoder provenance and local change

Upstream: https://android.googlesource.com/platform/external/libldac/
Revision: eeee1a3f5f8df1282e3a6d297085885fd886737b
Sony's LICENSE and NOTICE are preserved in libldac/.

One local source change in src/pack_ldac.c: cast idata to unsigned int before
left-shifting its bit pattern. UBSan found a shift of a negative signed integer
in the upstream implementation during real audio encoding. The masked result
uses the low 24 bits, and shifting the unsigned representation gives defined
C semantics. The source comment identifies the local modification.
