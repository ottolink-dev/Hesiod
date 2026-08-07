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
constexpr const char *P_DEPTH1      = "depth1";
constexpr const char *P_DEPTH2      = "depth2";
constexpr const char *P_WATER_DEPTH = "water_depth";

constexpr const char *A_K_SMOOTH = "k_smooth";

void setup_merge_water_depths_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DEPTH1);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DEPTH2);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_WATER_DEPTH, CONFIG(node));

  // attribute(s)
  add_float(node, A_K_SMOOTH, "k_smooth", 0.f, 0.f, 0.1f, "{:.4f}");
}

void compute_merge_water_depths_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in1 = node.get_value_ref<hmap::VirtualArray>(P_DEPTH1);
  hmap::VirtualArray *p_in2 = node.get_value_ref<hmap::VirtualArray>(P_DEPTH2);

  if (p_in1 && p_in2)
  {
    hmap::VirtualArray *p_depth = node.get_value_ref<hmap::VirtualArray>(P_WATER_DEPTH);

    hmap::for_each_tile(
        {p_depth, p_in1, p_in2},
        [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          hmap::Array *pa_depth = p_arrays[0];
          hmap::Array *pa_in1   = p_arrays[1];
          hmap::Array *pa_in2   = p_arrays[2];

          *pa_depth = hmap::merge_water_depths(*pa_in1,
                                               *pa_in2,
                                               node.val<float>(A_K_SMOOTH));
        },
        node.cfg().cm_cpu);

    p_depth->smooth_overlap_buffers();
  }
}

} // namespace hesiod
