# FastNoiseSIMD (vendored)

- Upstream: https://github.com/Auburn/FastNoiseSIMD
- Commit: `19dc220fba7500d3910cb18f22195b49f4d944cf` (header version 0.7.0)
- License: MIT, see `LICENSE`

Only the sources needed for x86-64 (SSE2 / SSE4.1 / AVX2) and ARM64 (NEON) are
kept. The files are unmodified; `CMakeLists.txt` is local and sets the per-file
code generation flags.

Used by the `MountainMassif` node (`Hesiod/src/model/nodes/landscape/`), whose
ridge and warp stages depend on this exact noise implementation.
