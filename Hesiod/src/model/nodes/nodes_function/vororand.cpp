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

void setup_vororand_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "dx");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "dy");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "envelope");
  node.add_port<hmap::Cloud>(gnode::PortType::IN, "cloud");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "out", CONFIG(node));

  // attribute(s)
  node.add_attr<EnumAttribute>("return_type",
                               "return_type",
                               enum_mappings.voronoi_return_type_map,
                               "F1: squared distance to the closest point");
  node.add_attr<FloatAttribute>("density", "density", 1.f, 0.f, 100.f);
  node.add_attr<FloatAttribute>("variability", "variability", 1.f, 1.f, 10.f);
  node.add_attr<SeedAttribute>("seed", "Seed");
  node.add_attr<FloatAttribute>("k_smoothing", "k_smoothing", 0.f, 0.f, 1.f);
  node.add_attr<FloatAttribute>("exp_sigma", "exp_sigma", 0.1f, 0.f, 0.3f);
  node.add_attr<BoolAttribute>("sqrt_output", "sqrt_output", false);

  // attribute(s) order
  node.set_attr_ordered_key({"return_type",
                             "density",
                             "variability",
                             "seed",
                             "k_smoothing",
                             "exp_sigma",
                             "sqrt_output"});

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = false, .remap_active_state = true});
}

void compute_vororand_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // base noise function
  hmap::VirtualArray *p_dx = node.get_value_ref<hmap::VirtualArray>("dx");
  hmap::VirtualArray *p_dy = node.get_value_ref<hmap::VirtualArray>("dy");
  hmap::VirtualArray *p_env = node.get_value_ref<hmap::VirtualArray>("envelope");
  hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("out");

  hmap::Cloud *p_cloud = node.get_value_ref<hmap::Cloud>("cloud");

  hmap::for_each_tile(
      {p_out, p_dx, p_dy},
      [&node, p_cloud](std::vector<hmap::Array *> p_arrays,
                       const hmap::TileRegion    &region)
      {
        auto [pa_out, pa_dx, pa_dy] = unpack<3>(p_arrays);

        hmap::VoronoiReturnType rtype = (hmap::VoronoiReturnType)
                                            node.get_attr<EnumAttribute>("return_type");

        if (p_cloud)
        {
          if (p_cloud->size() > 0)
          {
            *pa_out = hmap::gpu::vororand(region.shape,
                                          p_cloud->get_x(),
                                          p_cloud->get_y(),
                                          node.get_attr<FloatAttribute>("k_smoothing"),
                                          node.get_attr<FloatAttribute>("exp_sigma"),
                                          rtype,
                                          pa_dx,
                                          pa_dy,
                                          region.bbox);
          }
        }
        else
        {
          *pa_out = hmap::gpu::vororand(region.shape,
                                        node.get_attr<FloatAttribute>("density"),
                                        node.get_attr<FloatAttribute>("variability"),
                                        node.get_attr<SeedAttribute>("seed"),
                                        node.get_attr<FloatAttribute>("k_smoothing"),
                                        node.get_attr<FloatAttribute>("exp_sigma"),
                                        rtype,
                                        pa_dx,
                                        pa_dy,
                                        region.bbox);
        }
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
