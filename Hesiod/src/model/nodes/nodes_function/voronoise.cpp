/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/primitives.hpp"

#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_voronoise_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "dx");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "dy");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "envelope");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<WaveNbAttribute>("kw", "Spatial Frequency");
  node.add_attr<SeedAttribute>("seed", "Seed");
  node.add_attr<FloatAttribute>("u", "u", 0.5f, 0.f, 1.f);
  node.add_attr<FloatAttribute>("v", "v", 0.5f, 0.f, 1.f);

  // attribute(s) order
  node.set_attr_ordered_key({"kw", "seed", "u", "v"});

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = false, .remap_active_state = true});
}

void compute_voronoise_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");
  hmap::VirtualArray *p_dx = node.get_value_ref<hmap::VirtualArray>("dx");
  hmap::VirtualArray *p_dy = node.get_value_ref<hmap::VirtualArray>("dy");
  hmap::VirtualArray *p_env = node.get_value_ref<hmap::VirtualArray>("envelope");

  hmap::for_each_tile(
      {p_out, p_dx, p_dy},
      [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &region)
      {
        auto [pa_out, pa_dx, pa_dy] = unpack<3>(p_arrays);

        *pa_out = hmap::gpu::voronoise(region.shape,
                                       node.get_attr<WaveNbAttribute>("kw"),
                                       node.get_attr<FloatAttribute>("u"),
                                       node.get_attr<FloatAttribute>("v"),
                                       node.get_attr<SeedAttribute>("seed"),
                                       pa_dx,
                                       pa_dy,
                                       region.bbox);
      },
      node.cfg().cm_gpu);

  // post-process
  post_apply_enveloppe(node, *p_out, p_env);
  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
