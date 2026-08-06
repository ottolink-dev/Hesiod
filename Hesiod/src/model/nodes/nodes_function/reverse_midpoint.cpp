/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/authoring.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_reverse_midpoint_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Path>(gnode::PortType::IN, "path");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "heightmap", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("noise_scale", "noise_scale", 1.f, 0.01f, 2.f);
  node.add_attr<SeedAttribute>("seed", "Seed");

  // attribute(s) order
  node.set_attr_ordered_key({"noise_scale", "seed"});

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = false, .remap_active_state = true});
}

void compute_reverse_midpoint_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Path *p_path = node.get_value_ref<hmap::Path>("path");

  if (p_path)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("heightmap");

    if (p_path->size() > 1)
    {
      hmap::Array path_array = hmap::Array(p_out->shape);
      glm::vec4   bbox = glm::vec4(0.f, 1.f, 0.f, 1.f);
      p_path->to_array(path_array, bbox);

      hmap::Array z = hmap::reverse_midpoint(path_array,
                                             node.get_attr<SeedAttribute>("seed"),
                                             node.get_attr<FloatAttribute>("noise_scale"),
                                             0.f); // threshold

      p_out->from_array(z, node.cfg().cm_cpu);

      // post-process
      post_process_heightmap(node, *p_out);
    }
    else
    {
      // fill with zeros
      hmap::for_each_tile(
          {p_out},
          [](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
          {
            hmap::Array *pa_out = p_arrays[0];
            *pa_out = 0.f;
          },
          node.cfg().cm_cpu);
    }
  }
}

} // namespace hesiod
