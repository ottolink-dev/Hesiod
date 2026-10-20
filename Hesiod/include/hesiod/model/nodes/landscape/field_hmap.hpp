/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */

/**
 * @file field_hmap.hpp
 * @brief Conversions between hmap::Array and the square power-of-two Field grid
 * (same i + j * nx memory layout).
 */
#pragma once
#include <algorithm>

#include "highmap/array.hpp"

#include "hesiod/model/nodes/landscape/field.hpp"

namespace hesiod::landscape
{

// smallest power of two covering the array shape (at least 16)
inline int working_size(glm::ivec2 shape)
{
  int m = std::max({shape.x, shape.y, 16});
  int n = 16;
  while (n < m)
    n <<= 1;
  return n;
}

inline Field to_field(const hmap::Array &a, int n)
{
  Field f;
  f.n = n;
  if (a.shape.x == n && a.shape.y == n)
    f.data = a.vector;
  else
    f.data = a.resample_to_shape_bilinear(glm::ivec2(n, n)).vector;
  return f;
}

inline hmap::Array to_array(Field f, glm::ivec2 shape)
{
  hmap::Array a(glm::ivec2(f.n, f.n));
  a.vector = std::move(f.data);
  if (a.shape != shape)
    return a.resample_to_shape_bilinear(shape);
  return a;
}

} // namespace hesiod::landscape
