# HighMap #597: zero-width smooth clamps

Public range.hpp documents smoothing width k in [0, 1]. All six min/max/scalar/array-bound/combined overloads previously divided by zero at 0. Zero now dispatches to the corresponding existing hard clamp; positive-width formulas are unchanged. Canyonize already dispatches zero to hard clamp and was not affected.

21/21 Range tests pass, including exact hard-bound values at/below/above thresholds for every smooth-clamp overload and explicit IEEE finite checks. The same 21 tests pass Valgrind with zero errors and zero definite/indirect/possible leaks. Thirty positive-width cases (six overloads at .01/.1/.2/.5/1, 23,040 float cells) remain byte-identical to immutable d73 baseline; JSON records hashes and probe source is retained. Production range.cpp uses original release -Ofast/-ffast-math flags.
