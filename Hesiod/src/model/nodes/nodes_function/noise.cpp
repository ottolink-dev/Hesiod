/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/primitives.hpp"
#include "highmap/virtual_array/virtual_array.hpp"

#include "attributes.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_noise_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "dx");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "dy");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "envelope");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "out", CONFIG(node));

  // attribute(s)
  node.add_attr<EnumAttribute>("noise_type", "Type", enum_mappings.noise_type_map);
  node.add_attr<WaveNbAttribute>("kw", "Spatial Frequency");
  node.add_attr<SeedAttribute>("seed", "Seed");
  node.add_attr<BoolAttribute>("periodic", "Periodic (tileable)", false);

  // attribute(s) order
  node.set_attr_ordered_key(
      {"noise_type", "_SEPARATOR_", "kw", "seed", "periodic"});

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = false, .remap_active_state = true});
}

void compute_noise_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // base noise function
  hmap::VirtualArray *p_dx = node.get_value_ref<hmap::VirtualArray>("dx");
  hmap::VirtualArray *p_dy = node.get_value_ref<hmap::VirtualArray>("dy");
  hmap::VirtualArray *p_env = node.get_value_ref<hmap::VirtualArray>("envelope");
  hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("out");

  hmap::for_each_tile(
      {p_out, p_dx, p_dy},
      [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &region)
      {
        auto [pa_out, pa_dx, pa_dy] = unpack<3>(p_arrays);

        // when periodic, snap kw to integer cells so the lattice wrap (period)
        // aligns with the noise frequency and the result tiles seamlessly
        glm::vec2  kw = node.get_attr<WaveNbAttribute>("kw");
        glm::ivec2 period(-1, -1);
        if (node.get_attr<BoolAttribute>("periodic"))
        {
          kw = glm::vec2((float)(int)(kw.x + 0.5f), (float)(int)(kw.y + 0.5f));
          period = glm::ivec2((int)kw.x, (int)kw.y);
        }

        *pa_out = hmap::gpu::noise(
            (hmap::NoiseType)node.get_attr<EnumAttribute>("noise_type"),
            region.shape,
            kw,
            node.get_attr<SeedAttribute>("seed"),
            pa_dx,
            pa_dy,
            nullptr,
            region.bbox,
            period);
      },
      node.cfg().cm_gpu);

  // post-process
  post_apply_enveloppe(node, *p_out, p_env);
  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
