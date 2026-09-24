/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/gradient.hpp"
#include "highmap/morphology.hpp"
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/selector.hpp"

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

constexpr const char *P_IN  = "input";
constexpr const char *P_OUT = "output";

constexpr const char *G_SLOPE          = "Slope";
constexpr const char *G_ANGLE          = "Angle";
constexpr const char *G_INWARD_OUTWARD = "Inward / Outward";

constexpr const char *A_ANGLE          = "angle";
constexpr const char *A_CENTER         = "center";
constexpr const char *A_RADIUS         = "radius";
constexpr const char *A_SIGMA          = "sigma";
constexpr const char *A_MIN_MAX_KERNEL = "min_max_kernel";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_select_slope_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Group 1: Slope

  {
    node.set_current_group(G_SLOPE);

    node.set_current_category("Slope Parameters");
    add_float(node, A_RADIUS, "Radius", 0.f, 0.f, 1.f);

    setup_post_process_heightmap_attributes(
        node,
        {.add_mix = false, .remap_active_state = true});

    node.set_current_category("Advanced");
    add_enum(node,
             A_MIN_MAX_KERNEL,
             "Kernel Type",
             enum_mappings.min_max_kernel_map,
             "Octagon");
  }

  // --- Group 2: Angle

  {
    node.set_current_group(G_ANGLE);

    node.set_current_category("Angle Parameters");
    add_float(node, A_ANGLE, "Angle", 0.f, 0.f, 360.f);
    add_float(node, A_SIGMA, "Sigma", 90.f, 0.f, 180.f);
    add_float(node, A_RADIUS, "Radius", 0.f, 0.f, 0.2f);

    setup_post_process_heightmap_attributes(
        node,
        {.add_mix = true, .remap_active_state = false});
  }

  // --- Group 3: Inward / Outward

  {
    node.set_current_group(G_INWARD_OUTWARD);

    node.set_current_category("Direction Parameters");
    add_xy(node, A_CENTER, "Center");

    setup_post_process_heightmap_attributes(
        node,
        {.add_mix = false, .remap_active_state = true});
  }

  // Reset active group to first
  node.set_current_group(G_SLOPE);
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_select_slope_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_in  = node.get_value_ref<hmap::VirtualArray>(P_IN);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_in || !p_out)
    return;

  const std::string group = node.get_meta_group().current_container_name().value_or(
      G_SLOPE);

  // --- Group Dispatch

  if (group == G_SLOPE)
  {
    const auto kernel_type = node.val_enum<hmap::MinMaxKernel>(A_MIN_MAX_KERNEL);
    int        ir          = node.val_pixel_radius(A_RADIUS, 0);

    if (ir > 0)
    {
      hmap::for_each_tile(
          {p_out, p_in},
          [&node, ir, kernel_type](std::vector<hmap::Array *> p_arrays,
                                   const hmap::TileRegion &)
          {
            hmap::Array *pa_out = p_arrays[0];
            hmap::Array *pa_in  = p_arrays[1];

            *pa_out = hmap::gpu::morphological_gradient(*pa_in, ir, kernel_type);
          },
          node.cfg().cm_gpu);
    }
    else
    {
      hmap::for_each_tile(
          {p_out, p_in},
          [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
          {
            hmap::Array *pa_out = p_arrays[0];
            hmap::Array *pa_in  = p_arrays[1];

            *pa_out = hmap::gradient_norm(*pa_in);
          },
          node.cfg().cm_cpu);
    }

    p_out->smooth_overlap_buffers();
    post_process_heightmap(node, *p_out);
  }
  else if (group == G_ANGLE)
  {
    int ir = node.val_pixel_radius(A_RADIUS, 0);

    hmap::for_each_tile(
        {p_out, p_in},
        [&node, &ir](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);
          *pa_out              = select_angle(*pa_in,
                                 node.val<float>(A_ANGLE),
                                 node.val<float>(A_SIGMA),
                                 ir);
        },
        node.cfg().cm_cpu);

    p_out->smooth_overlap_buffers();
    post_process_heightmap(node, *p_out);
  }
  else if (group == G_INWARD_OUTWARD)
  {
    hmap::for_each_tile(
        {p_out, p_in},
        [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &region)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);
          *pa_out              = hmap::select_inward_outward_slope(*pa_in,
                                                      node.val<glm::vec2>(A_CENTER),
                                                      region.bbox);
        },
        node.cfg().cm_cpu);

    p_out->smooth_overlap_buffers();
    post_process_heightmap(node, *p_out);
  }
}

} // namespace hesiod
