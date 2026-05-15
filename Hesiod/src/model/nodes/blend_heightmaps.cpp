/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/blending.hpp"
#include "highmap/range.hpp"

#include "hesiod/model/nodes/base_node.hpp"

namespace hesiod
{

void blend_heightmaps(BaseNode           &node,
                      hmap::VirtualArray &h_out,
                      hmap::VirtualArray &h1,
                      hmap::VirtualArray &h2,
                      BlendingMethod      method,
                      float               k,
                      int                 ir,
                      float               w1,
                      float               w2)
{
  std::function<void(hmap::Array &, hmap::Array &, hmap::Array &)> lambda;

  switch (method)
  {
  case BlendingMethod::ADD:
    lambda = [w1, w2](hmap::Array &m, hmap::Array &a1, hmap::Array &a2)
    { m = w1 * a1 + w2 * a2; };
    break;

  case BlendingMethod::EXCLUSION_BLEND:
    lambda = [w1, w2](hmap::Array &m, hmap::Array &a1, hmap::Array &a2)
    { m = hmap::blend_exclusion(w1 * a1, w2 * a2); };
    break;

  case BlendingMethod::GRADIENTS:
    lambda = [ir, w1, w2](hmap::Array &m, hmap::Array &a1, hmap::Array &a2)
    { m = hmap::blend_gradients(w1 * a1, w2 * a2, ir); };
    break;

  case BlendingMethod::MAXIMUM:
    lambda = [w1, w2](hmap::Array &m, hmap::Array &a1, hmap::Array &a2)
    { m = hmap::maximum(w1 * a1, w2 * a2); };
    break;

  case BlendingMethod::MAXIMUM_SMOOTH:
    lambda = [k, w1, w2](hmap::Array &m, hmap::Array &a1, hmap::Array &a2)
    { m = hmap::maximum_smooth(w1 * a1, w2 * a2, k); };
    break;

  case BlendingMethod::MINIMUM:
    lambda = [w1, w2](hmap::Array &m, hmap::Array &a1, hmap::Array &a2)
    { m = hmap::minimum(w1 * a1, w2 * a2); };
    break;

  case BlendingMethod::MINIMUM_SMOOTH:
    lambda = [k, w1, w2](hmap::Array &m, hmap::Array &a1, hmap::Array &a2)
    { m = hmap::minimum_smooth(w1 * a1, w2 * a2, k); };
    break;

  case BlendingMethod::MULTIPLY:
    lambda = [w1, w2](hmap::Array &m, hmap::Array &a1, hmap::Array &a2)
    { m = w1 * a1 * w2 * a2; };
    break;

  case BlendingMethod::MULTIPLY_ADD:
    lambda = [w1, w2](hmap::Array &m, hmap::Array &a1, hmap::Array &a2)
    { m = w1 * a1 + w1 * a1 * w2 * a2; };
    break;

  case BlendingMethod::NEGATE:
    lambda = [w1, w2](hmap::Array &m, hmap::Array &a1, hmap::Array &a2)
    { m = hmap::blend_negate(w1 * a1, w2 * a2); };
    break;

  case BlendingMethod::OVERLAY:
    lambda = [w1, w2](hmap::Array &m, hmap::Array &a1, hmap::Array &a2)
    { m = hmap::blend_overlay(w1 * a1, w2 * a2); };
    break;

  case BlendingMethod::SOFT:
    lambda = [w1, w2](hmap::Array &m, hmap::Array &a1, hmap::Array &a2)
    { m = hmap::blend_soft(w1 * a1, w2 * a2); };
    break;

  case BlendingMethod::REPLACE:
    lambda = [w1, w2](hmap::Array &m, hmap::Array & /* a1 */, hmap::Array &a2)
    { m = w2 * a2; };
    break;

  case BlendingMethod::SUBSTRACT:
    lambda = [w1, w2](hmap::Array &m, hmap::Array &a1, hmap::Array &a2)
    { m = w1 * a1 - w2 * a2; };
    break;
  }

  auto lambda_tile =
      [&lambda](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
  {
    if (p_arrays.size() >= 3)
    {
      hmap::Array &m = *p_arrays[0];
      hmap::Array &a1 = *p_arrays[1];
      hmap::Array &a2 = *p_arrays[2];
      lambda(m, a1, a2);
    }
  };

  hmap::for_each_tile({&h_out, &h1, &h2}, lambda_tile, node.cfg().cm_cpu);

  if (method == BlendingMethod::GRADIENTS)
    h_out.smooth_overlap_buffers();
}

} // namespace hesiod
