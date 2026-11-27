# HighMap #592: reliable approximate comparisons

Baseline repro output 1,1,1,0 means NaN-vs-finite, Inf-vs-Inf and zeros-vs-ones were accepted, while ones-vs-zeros failed. The helper is shared by 172 test call sites.

The fix rejects nonfinite inputs/tolerances using IEEE exponent bits, which remain meaningful under the release -Ofast/-ffast-math compilation; accumulates differences and magnitudes in double to avoid overflow for finite float arrays; applies an absolute difference to zero-field comparisons; and resets reused success diagnostics. The finite nonzero relative L1 formula is unchanged.

Six new focused tests cover nonfinite operands/tolerances, symmetric zero fields, existing relative tolerance semantics, finite extreme-value overflow, and reused diagnostics. All 25 combined Quilting/SmoothCPulse/assertion tests pass against changed production objects with the exact release flags. The six assertion tests pass Valgrind with zero errors and zero definite/indirect/possible leaks. Full HighMap validation is owned by the integration test agent; new failures after repairing this helper must be exposed and triaged.
