/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/primitives.hpp"

#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_voronoi_fbm_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "dx");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "dy");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "control");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "envelope");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<EnumAttribute>("return_type",
                               "",
                               enum_mappings.voronoi_return_type_map,
                               "F1: squared distance to the closest point");
  node.add_attr<WaveNbAttribute>("kw", "Spatial Frequency");
  node.add_attr<SeedAttribute>("seed", "Seed");
  node.add_attr<FloatAttribute>("jitter.x", "jitter.x", 1.f, 0.f, 1.f);
  node.add_attr<FloatAttribute>("jitter.y", "jitter.y", 1.f, 0.f, 1.f);
  node.add_attr<FloatAttribute>("k_smoothing", "k_smoothing", 0.5f, 0.f, 1.f);
  node.add_attr<FloatAttribute>("exp_sigma", "exp_sigma", 0.1f, 0.f, 0.3f);
  node.add_attr<BoolAttribute>("sqrt_output", "sqrt_output", false);
  node.add_attr<IntAttribute>("octaves", "Octaves", 8, 0, 32);
  node.add_attr<FloatAttribute>("weight", "Weight", 0.7f, 0.f, 1.f);
  node.add_attr<FloatAttribute>("persistence", "Persistence", 0.5f, 0.f, 1.f);
  node.add_attr<FloatAttribute>("lacunarity", "Lacunarity", 2.f, 0.01f, 4.f);

  // attribute(s) order
  node.set_attr_ordered_key({"_GROUPBOX_BEGIN_Noise Type",
                             "return_type",
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Frequency and Structure",
                             "kw",
                             "seed",
                             "jitter.x",
                             "jitter.y",
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Output Shaping",
                             "k_smoothing",
                             "exp_sigma",
                             "sqrt_output",
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Fractal Layers",
                             "octaves",
                             "weight",
                             "persistence",
                             "lacunarity",
                             "_GROUPBOX_END_"});

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = false, .remap_active_state = true});
}

void compute_voronoi_fbm_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // base voronoi function
  hmap::VirtualArray *p_dx = node.get_value_ref<hmap::VirtualArray>("dx");
  hmap::VirtualArray *p_dy = node.get_value_ref<hmap::VirtualArray>("dy");
  hmap::VirtualArray *p_ctrl = node.get_value_ref<hmap::VirtualArray>("control");
  hmap::VirtualArray *p_env = node.get_value_ref<hmap::VirtualArray>("envelope");
  hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");

  hmap::for_each_tile(
      {p_out, p_dx, p_dy, p_ctrl},
      [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &region)
      {
        auto [pa_out, pa_dx, pa_dy, pa_ctrl] = unpack<4>(p_arrays);

        hmap::VoronoiReturnType rtype = (hmap::VoronoiReturnType)
                                            node.get_attr<EnumAttribute>("return_type");

        glm::vec2 jitter(node.get_attr<FloatAttribute>("jitter.x"),
                         node.get_attr<FloatAttribute>("jitter.y"));

        *pa_out = hmap::gpu::voronoi_fbm(region.shape,
                                         node.get_attr<WaveNbAttribute>("kw"),
                                         node.get_attr<SeedAttribute>("seed"),
                                         jitter,
                                         node.get_attr<FloatAttribute>("k_smoothing"),
                                         node.get_attr<FloatAttribute>("exp_sigma"),
                                         rtype,
                                         node.get_attr<IntAttribute>("octaves"),
                                         node.get_attr<FloatAttribute>("weight"),
                                         node.get_attr<FloatAttribute>("persistence"),
                                         node.get_attr<FloatAttribute>("lacunarity"),
                                         pa_ctrl,
                                         pa_dx,
                                         pa_dy,
                                         region.bbox);
      },
      node.cfg().cm_gpu);

  // apply square root
  p_out->remap(0.f, 1.f, node.cfg().cm_cpu);

  if (node.get_attr<BoolAttribute>("sqrt_output"))
    hmap::for_each_tile(
        {p_out},
        [](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          hmap::Array *pa_out = p_arrays[0];
          *pa_out = hmap::sqrt(*pa_out);
        },
        node.cfg().cm_cpu);

  // post-process
  post_apply_enveloppe(node, *p_out, p_env);
  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
