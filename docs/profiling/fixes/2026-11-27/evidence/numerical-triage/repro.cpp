#include "highmap/array.hpp"
#include "highmap/filters.hpp"
#include "highmap/morphology.hpp"
#include <bit>
#include <cstdint>
#include <iostream>

int main()
{
  hmap::Array input{{42.f}}, reference{{7.f}};
  hmap::match_histogram(input, reference);
  const auto bits = std::bit_cast<std::uint32_t>(input.vector[0]);
  const bool finite = (bits & 0x7f800000u) != 0x7f800000u;
  std::cout << "singleton value=" << input.vector[0] << " bits=" << bits
            << " finite=" << finite << " equals7=" << (input.vector[0] == 7.f) << '\n';

  hmap::Array line{{0.f, 0.f, 0.f, 0.f, 0.f},
                   {0.f, 1.f, 1.f, 1.f, 1.f},
                   {0.f, 0.f, 0.f, 0.f, 0.f}};
  for (int iterations = 0; iterations <= 2; ++iterations)
  {
    const auto result = hmap::remove_endpoints(line, iterations, 0.f);
    int foreground = 0;
    for (float value : result.vector) foreground += value != 0.f;
    std::cout << "endpoints iterations=" << iterations << " foreground=" << foreground
              << " row=";
    for (int x = 0; x < result.shape.x; ++x) std::cout << result(x, 1) << ',';
    std::cout << '\n';
    if (foreground != 4 - 2 * iterations) return 2;
  }
  return finite && input.vector[0] == 7.f ? 0 : 1;
}
