// Deterministic integration workload for the shared HighMap dispatch paths.
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <thread>

#include "hesiod/profiling.hpp"
#include "highmap/virtual_array/virtual_array.hpp"

int main()
{
#ifdef HESIOD_ENABLE_TRACY
  if (const char *wait = std::getenv("HESIOD_TRACY_WAIT");
      wait && std::string_view(wait) == "1")
  {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (!TracyIsConnected && std::chrono::steady_clock::now() < deadline)
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    if (!TracyIsConnected)
      return 2;
  }
#endif
  const hmap::ComputeMode sequential{.mode = hmap::VA_SEQUENTIAL};
  for (bool reduced : {false, true})
    for (auto mode : {hmap::VA_SEQUENTIAL,
                      hmap::VA_DISTRIBUTED,
                      hmap::VA_SINGLE_ARRAY})
    {
      HSD_PROFILE_ZONE("HighMap workload::dispatch_mode");
      HSD_PROFILE_TEXT(hmap::to_string(mode));
      hmap::VirtualArray input({32, 24}, {16, 12}, 0);
      hmap::VirtualArray output({32, 24}, {16, 12}, 0);
      // Tiled callbacks preserve a ramp. Full-array modes resample even at
      // native resolution, so use a constant to check their reconstruction.
      hmap::Array source(glm::ivec2(32, 24));
      for (size_t i = 0; i < source.vector.size(); ++i)
        source.vector[i] = (mode == hmap::VA_SINGLE_ARRAY)
                               ? 2.f
                               : static_cast<float>(i) / 1024.f;
      input.from_array(source, sequential);
      output.fill(0.f, sequential);
      hmap::ComputeMode cm{.mode = mode,
                           .trim_storage = true,
                           .stride = reduced ? 2 : 1};
      hmap::for_each_tile(
          {&input},
          {&output},
          [](const std::vector<const hmap::Array *> &in,
             std::vector<hmap::Array *>             &out,
             const hmap::TileRegion &) { *out[0] = *in[0] + 3.f; },
          cm);
      auto result = output.to_array(sequential);
      for (size_t i = 0; i < result.vector.size(); ++i)
        if (std::abs(result.vector[i] - (source.vector[i] + 3.f)) > 1e-5f)
        {
          std::cerr << "Incorrect output for " << hmap::to_string(mode) << '\n';
          return 1;
        }
      std::cout << hmap::to_string(mode) << (reduced ? " reduced" : " native")
                << ": all 768 cells match\n";
    }
  return 0;
}
