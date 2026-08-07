/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/erosion.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_hydraulic_blur_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("radius", "radius", 0.1f, 0.01f, 0.5f);
  node.add_attr<FloatAttribute>("vmax", "vmax", 0.5f, -1.f, 2.f);
  node.add_attr<FloatAttribute>("k_smoothing", "k_smoothing", 0.1f, 0.f, 1.f);

  // attribute(s) order
  node.set_attr_ordered_key({"radius", "vmax", "k_smoothing"});

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

void compute_hydraulic_blur_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  auto *p_in  = node.get_value_ref<hmap::VirtualArray>("input");
  auto *p_out = node.get_value_ref<hmap::VirtualArray>("output");

  if (!p_in)
    return;

  float zmin = p_in->min(node.cfg().cm_cpu);
  float zmax = p_in->max(node.cfg().cm_cpu);

  hmap::for_each_tile(
      {p_out, p_in},
      [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
      {
        auto [pa_out, pa_in] = unpack<2>(p_arrays);
        *pa_out              = *pa_in;

        hmap::hydraulic_blur(*pa_out,
                             node.get_attr<FloatAttribute>("radius"),
                             node.get_attr<FloatAttribute>("vmax"),
                             node.get_attr<FloatAttribute>("k_smoothing"));
      },
      node.cfg().cm_cpu);

  // preverse input elevation range
  p_out->remap(zmin, zmax, node.cfg().cm_cpu);

  // post-process
  p_out->smooth_overlap_buffers();
  post_process_heightmap(node, *p_out, p_in);
}

} // namespace hesiod
