/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/authoring.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_CLOUD  = "cloud";
constexpr const char *P_KERNEL = "kernel";
constexpr const char *P_OUT    = "output";

constexpr const char *A_BLEND_METHOD           = "blend_method";
constexpr const char *A_K_SMOOTHING            = "k_smoothing";
constexpr const char *A_KERNEL_FLIP            = "kernel_flip";
constexpr const char *A_KERNEL_RADIUS          = "kernel_radius";
constexpr const char *A_KERNEL_ROTATE          = "kernel_rotate";
constexpr const char *A_KERNEL_SCALE_AMPLITUDE = "kernel_scale_amplitude";
constexpr const char *A_KERNEL_SCALE_RADIUS    = "kernel_scale_radius";
constexpr const char *A_SEED                   = "seed";

void setup_stamping_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Cloud>(gnode::PortType::IN, P_CLOUD);
  node.add_port<hmap::Array>(gnode::PortType::IN, P_KERNEL);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  add_float(node, A_KERNEL_RADIUS, "kernel_radius", 0.1f, 0.01f, 0.5f);
  add_bool(node, A_KERNEL_SCALE_RADIUS, "kernel_scale_radius", false);
  add_bool(node, A_KERNEL_SCALE_AMPLITUDE, "kernel_scale_amplitude", true);
  add_enum(node,
           A_BLEND_METHOD,
           "blend_method",
           enum_mappings.stamping_blend_method_map,
           "maximum");
  add_seed(node, A_SEED, "Seed");
  add_float(node, A_K_SMOOTHING, "k_smoothing", 0.1f, 0.01f, 1.f);
  add_bool(node, A_KERNEL_FLIP, "kernel_flip", false);
  add_bool(node, A_KERNEL_ROTATE, "kernel_rotate", false);
  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = false, .remap_active_state = true});
}

void compute_stamping_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Cloud *p_cloud  = node.get_value_ref<hmap::Cloud>(P_CLOUD);
  hmap::Array *p_kernel = node.get_value_ref<hmap::Array>(P_KERNEL);

  if (p_cloud && p_kernel)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

    std::vector<float> xp = p_cloud->get_x();
    std::vector<float> yp = p_cloud->get_y();
    std::vector<float> zp = p_cloud->get_values();

    int  ir   = std::max(1, (int)(node.val<float>(A_KERNEL_RADIUS) * p_out->shape.x));
    uint seed = node.val<int>(A_SEED);

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
              node.val<bool>(A_KERNEL_SCALE_RADIUS),
              node.val<bool>(A_KERNEL_SCALE_AMPLITUDE),
              (hmap::StampingBlendMethod)node.val<int>(A_BLEND_METHOD),
              seed++,
              node.val<float>(A_K_SMOOTHING),
              node.val<bool>(A_KERNEL_FLIP),
              node.val<bool>(A_KERNEL_ROTATE),
              region.bbox);
        },
        node.cfg().cm_cpu);

    p_out->smooth_overlap_buffers();

    // post-process
    post_process_heightmap(node, *p_out);
  }
}

} // namespace hesiod
