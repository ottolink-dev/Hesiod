# HighMap #591: zero-radius smoothing

CPU and GPU smooth_cpulse now return unchanged after input/mask validation when radius is zero. This avoids the undefined kernel weight from dividing zero by zero. The GPU fast path requires no OpenCL device; positive radii retain the existing implementation.

Baseline repro: finite 48² input becomes 877 nonfinite cells through small-patch quilting_expand; direct smooth_cpulse(radius 0) produces 2304/2304 nonfinite cells. Regression coverage now checks exact values and finiteness with/without mask on CPU and GPU, plus finite small-quilting seams with rounded-zero radii. Existing assert_almost_equal had silently accepted NaNs; #592 separately addresses that helper.

19/19 targeted Quilting/SmoothCPulse tests pass. Six focused tests pass Valgrind with zero errors and zero definite/indirect/possible leaks (OpenMP limited to one thread).3,240 positive-radius/no-smoothing compatibility cases remain byte-identical; JSON records both hashes. The separate quilting memory evidence retains the original memory-only source state.
