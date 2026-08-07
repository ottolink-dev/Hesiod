/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/local_metrics.hpp"
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/range.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_area_remove_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.set_current_category("Metric Choice");
  add_float(node, "radius_limit", "Minimum Radius", 0.01f, 1e-3f, 0.5f, "{:.2e}", true);
  add_float(node, "bg_value", "Background Value", 0.f, -1.f, 1.f);
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_area_remove_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_in = node.get_value_ref<hmap::VirtualArray>("input");
  auto *p_out = node.get_value_ref<hmap::VirtualArray>("output");

  if (!p_in)
    return;

  // --- Params

  const auto radius   = node.val<float>("radius_limit");
  const auto bg_value = node.val<float>("bg_value");

  const float area_pixels = M_PI * std::pow(radius * p_in->shape.x, 2);

  // --- Compute

  hmap::for_each_tile(
      {p_out, p_in},
      [&](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
      {
        auto [pa_out, pa_in] = unpack<2>(p_arrays);

        *pa_out = hmap::area_remove(*pa_in, area_pixels, bg_value, bg_value);
      },
      node.cfg().cm_single_array);
}

} // namespace hesiod
