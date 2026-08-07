/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_CLOUD = "cloud";
constexpr const char *P_DX    = "dx";
constexpr const char *P_DY    = "dy";
constexpr const char *P_SDF   = "sdf";

void setup_cloud_sdf_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Cloud>(gnode::PortType::IN, P_CLOUD);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DX);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DY);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_SDF, CONFIG(node));

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = false, .remap_active_state = true});
}

void compute_cloud_sdf_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Cloud        *p_cloud = node.get_value_ref<hmap::Cloud>(P_CLOUD);
  hmap::VirtualArray *p_out   = node.get_value_ref<hmap::VirtualArray>(P_SDF);

  if (!p_cloud || p_cloud->size() == 0)
  {
    // fill with zeros
    hmap::for_each_tile(
        {p_out},
        [](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          hmap::Array *pa_out = p_arrays[0];
          *pa_out             = 0.f;
        },
        node.cfg().cm_cpu);
  }
  else
  {
    hmap::VirtualArray *p_dx = node.get_value_ref<hmap::VirtualArray>(P_DX);
    hmap::VirtualArray *p_dy = node.get_value_ref<hmap::VirtualArray>(P_DY);

    hmap::for_each_tile(
        {p_out, p_dx, p_dy},
        [&node, p_cloud](std::vector<hmap::Array *> p_arrays,
                         const hmap::TileRegion    &region)
        {
          hmap::Array *pa_out = p_arrays[0];
          hmap::Array *pa_dx  = p_arrays[1];
          hmap::Array *pa_dy  = p_arrays[2];

          *pa_out = hmap::cloud_sdf_to_array(*p_cloud,
                                             region.shape,
                                             region.bbox,
                                             pa_dx,
                                             pa_dy);
        },
        node.cfg().cm_cpu);

    // post-process
    post_process_heightmap(node, *p_out);
  }
}

} // namespace hesiod
