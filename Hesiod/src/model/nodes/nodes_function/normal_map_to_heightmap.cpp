/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/gradient.hpp"
#include "highmap/range.hpp"
#include "highmap/virtual_array/virtual_texture.hpp"

#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_normal_map_to_heightmap_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, "normal map");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<BoolAttribute>("poisson_solver", "poisson_solver", false);
  node.add_attr<IntAttribute>("iterations", "iterations", 500, 1, INT_MAX);
  node.add_attr<FloatAttribute>("omega", "omega", 1.5f, 1e-3f, 2.f);

  // attribute(s) order
  node.set_attr_ordered_key({"poisson_solver", "iterations", "omega"});

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = true});
}

void compute_normal_map_to_heightmap_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualTexture *p_nmap = node.get_value_ref<hmap::VirtualTexture>("normal map");

  if (p_nmap)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");

    hmap::Tensor ts = hmap::Tensor(p_nmap->shape, 3);
    for (int nch = 0; nch < 3; ++nch)
      ts.set_slice(nch, p_nmap->channel(nch).to_array(node.cfg().cm_cpu));

    hmap::Array z;

    if (node.get_attr<BoolAttribute>("poisson_solver"))
    {
      z = hmap::normal_map_to_heightmap_poisson(ts,
                                                node.get_attr<IntAttribute>("iterations"),
                                                node.get_attr<FloatAttribute>("omega"));
    }
    else
    {
      z = hmap::normal_map_to_heightmap(ts);
    }

    p_out->from_array(z, node.cfg().cm_cpu);
    p_out->remap(0.f, 1.f, node.cfg().cm_cpu);

    // post-process
    post_process_heightmap(node, *p_out);
  }
}

} // namespace hesiod
