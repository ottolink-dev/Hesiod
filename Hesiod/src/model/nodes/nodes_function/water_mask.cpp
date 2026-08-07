/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/hydrology/hydrology.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_water_mask_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "elevation");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "water_depth");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "mask", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("additional_depth",
                                "Additional Water Depth",
                                0.f,
                                0.f,
                                0.2f);

  // attribute(s) order
  node.set_attr_ordered_key(
      {"_GROUPBOX_BEGIN_Main Parameters", "additional_depth", "_GROUPBOX_END_"});

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = true});
}

void compute_water_mask_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_z     = node.get_value_ref<hmap::VirtualArray>("elevation");
  hmap::VirtualArray *p_depth = node.get_value_ref<hmap::VirtualArray>("water_depth");

  if (p_z && p_depth)
  {
    hmap::VirtualArray *p_mask = node.get_value_ref<hmap::VirtualArray>("mask");

    hmap::for_each_tile(
        {p_depth, p_z, p_mask},
        [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          hmap::Array *pa_depth = p_arrays[0];
          hmap::Array *pa_z     = p_arrays[1];
          hmap::Array *pa_mask  = p_arrays[2];

          float added_depth = node.get_attr<FloatAttribute>("additional_depth");

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
