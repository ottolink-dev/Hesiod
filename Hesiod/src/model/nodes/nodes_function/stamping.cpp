/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/authoring.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_stamping_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Cloud>(gnode::PortType::IN, "cloud");
  node.add_port<hmap::Array>(gnode::PortType::IN, "kernel");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("kernel_radius", "kernel_radius", 0.1f, 0.01f, 0.5f);
  node.add_attr<BoolAttribute>("kernel_scale_radius", "kernel_scale_radius", false);
  node.add_attr<BoolAttribute>("kernel_scale_amplitude", "kernel_scale_amplitude", true);
  node.add_attr<EnumAttribute>("blend_method",
                               "blend_method",
                               enum_mappings.stamping_blend_method_map,
                               "maximum");
  node.add_attr<SeedAttribute>("seed", "Seed");
  node.add_attr<FloatAttribute>("k_smoothing", "k_smoothing", 0.1f, 0.01f, 1.f);
  node.add_attr<BoolAttribute>("kernel_flip", "kernel_flip", false);
  node.add_attr<BoolAttribute>("kernel_rotate", "kernel_rotate", false);
  // attribute(s) order
  node.set_attr_ordered_key({"kernel_radius",
                             "kernel_scale_radius",
                             "kernel_scale_amplitude",
                             "blend_method",
                             "seed",
                             "k_smoothing",
                             "kernel_flip",
                             "kernel_rotate"});

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = false, .remap_active_state = true});
}

void compute_stamping_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Cloud *p_cloud = node.get_value_ref<hmap::Cloud>("cloud");
  hmap::Array *p_kernel = node.get_value_ref<hmap::Array>("kernel");

  if (p_cloud && p_kernel)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");

    std::vector<float> xp = p_cloud->get_x();
    std::vector<float> yp = p_cloud->get_y();
    std::vector<float> zp = p_cloud->get_values();

    int ir = std::max(
        1,
        (int)(node.get_attr<FloatAttribute>("kernel_radius") * p_out->shape.x));
    uint seed = node.get_attr<SeedAttribute>("seed");

    hmap::for_each_tile(
        {p_out},
        [&node, &xp, &yp, &zp, p_kernel, ir, &seed](std::vector<hmap::Array *> p_arrays,
                                                    const hmap::TileRegion    &region)
        {
          auto [pa_out] = unpack<1>(p_arrays);

          *pa_out = hmap::stamping(
              region.shape,
              xp,
              yp,
              zp,
              *p_kernel,
              ir,
              node.get_attr<BoolAttribute>("kernel_scale_radius"),
              node.get_attr<BoolAttribute>("kernel_scale_amplitude"),
              (hmap::StampingBlendMethod)node.get_attr<EnumAttribute>("blend_method"),
              seed++,
              node.get_attr<FloatAttribute>("k_smoothing"),
              node.get_attr<BoolAttribute>("kernel_flip"),
              node.get_attr<BoolAttribute>("kernel_rotate"),
              region.bbox);
        },
        node.cfg().cm_cpu);

    p_out->smooth_overlap_buffers();

    // post-process
    post_process_heightmap(node, *p_out);
  }
}

} // namespace hesiod
