/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */

/**
 * @file field.hpp
 * @brief Square float field shared by the landscape simulations (mountain
 * generator, multiscale erosion, snow cover).
 */
#pragma once
#include <cstddef>
#include <vector>

namespace hesiod::landscape
{

// square field, row-major: value(x, y) = data[y * n + x]
struct Field
{
  int                n = 0;
  std::vector<float> data;

  Field() = default;
  explicit Field(int size, float value = 0.f)
      : n(size), data(size_t(size) * size_t(size), value)
  {
  }

  float &at(int x, int y) { return data[size_t(y) * n + x]; }
  float  at(int x, int y) const { return data[size_t(y) * n + x]; }
};

} // namespace hesiod::landscape
