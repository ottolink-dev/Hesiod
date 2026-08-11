/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/hydrology/hydrology.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_DEPTH       = "depth";
constexpr const char *P_MASK        = "mask";
constexpr const char *P_WATER_DEPTH = "water_depth";

constexpr const char *A_DRY_OUT_RATIO = "dry_out_ratio";

void setup_water_depth_dry_out_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DEPTH);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_WATER_DEPTH, CONFIG(node));

  // attribute(s)
  add_float(node, A_DRY_OUT_RATIO, "dry_out_ratio", 0.1f, 0.f, 0.5f, "{:.4f}");
}

void compute_water_depth_dry_out_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(P_DEPTH);

  if (p_in)
  {
    hmap::VirtualArray *p_mask = node.get_value_ref<hmap::VirtualArray>(P_MASK);
    hmap::VirtualArray *p_out  = node.get_value_ref<hmap::VirtualArray>(P_WATER_DEPTH);

    float depth_max = p_in->max(node.cfg().cm_cpu);

    hmap::for_each_tile(
        {p_out, p_in, p_mask},
        [&node, depth_max](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out, pa_in, pa_mask] = unpack<3>(p_arrays);
          *pa_out                       = *pa_in;

          hmap::water_depth_dry_out(*pa_out,
                                    node.val<float>(A_DRY_OUT_RATIO),
                                    pa_mask,
                                    depth_max);
        },
        node.cfg().cm_cpu);
  }
}

} // namespace hesiod
