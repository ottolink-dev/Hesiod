# HighMap #596: PERLIN_HALF CPU/GPU parity

Unchanged d73 baseline 64² warped noise and noise_fbm each return 4096/4096 nonfinite CPU cells and 0/4096 nonfinite GPU cells. The old assertion helper incorrectly returns true. noise-repro.cpp/.log reproduce this independently of the repaired helper, using strict -O2 std::isfinite checks.

CPU factory formerly selected PerlinHalfFunction(k=0), which invokes a smooth-clamp division by zero; GPU kernels consistently use k=.5. The one-line factory fix matches that existing GPU behavior. No GPU algorithm is changed. Separate #597 fixes the public zero-width clamp contract.

Both existing CPU/GPU Noise and NoiseFbm tests now explicitly classify IEEE exponent bits to reject nonfinite cells even with fast-math. Both pass on NVIDIA GeForce RTX5060 Laptop GPU (OpenCL3.0 CUDA), covering 10 noise types each, with existing relative tolerance 1e-5. Source production object was compiled using its original release -Ofast/-ffast-math flags. Tests use the immutable baseline archive with only the changed CPU noise object and repaired assertion object overriding it.
