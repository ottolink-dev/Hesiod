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
constexpr const char *P_ELEVATION   = "elevation";
constexpr const char *P_MASK        = "mask";
constexpr const char *P_WATER_DEPTH = "water_depth";

constexpr const char *A_ADDITIONAL_DEPTH = "additional_depth";

void setup_water_mask_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ELEVATION);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_WATER_DEPTH);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_MASK, CONFIG(node));

  // attribute(s)
  add_float(node, A_ADDITIONAL_DEPTH, "Additional Water Depth", 0.f, 0.f, 0.2f);

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = true});
}

void compute_water_mask_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_z     = node.get_value_ref<hmap::VirtualArray>(P_ELEVATION);
  hmap::VirtualArray *p_depth = node.get_value_ref<hmap::VirtualArray>(P_WATER_DEPTH);

  if (p_z && p_depth)
  {
    hmap::VirtualArray *p_mask = node.get_value_ref<hmap::VirtualArray>(P_MASK);

    hmap::for_each_tile(
        {p_depth, p_z, p_mask},
        [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          hmap::Array *pa_depth = p_arrays[0];
          hmap::Array *pa_z     = p_arrays[1];
          hmap::Array *pa_mask  = p_arrays[2];

          float added_depth = node.val<float>(A_ADDITIONAL_DEPTH);

          if (added_depth)
          {
            *pa_mask = hmap::water_mask(*pa_depth, *pa_z, added_depth);
          }
          else
          {
            *pa_mask = hmap::water_mask(*pa_depth);
          }
        },
        node.cfg().cm_gpu);

    // post-process
    p_mask->smooth_overlap_buffers();
    post_process_heightmap(node, *p_mask);
  }
}

} // namespace hesiod
