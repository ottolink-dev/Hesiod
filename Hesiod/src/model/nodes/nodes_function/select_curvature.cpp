/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/filters.hpp"
#include "highmap/geometry/grids.hpp"
#include "highmap/morphology.hpp"
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/selector.hpp"

#include "hesiod/model/nodes/attributes.hpp"

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

constexpr const char *G_CAVITIES = "Cavities";
constexpr const char *G_VALLEY   = "Valley";
constexpr const char *G_BLOB_LOG = "Blob (LoG)";

constexpr const char *A_CONCAVE      = "concave";
constexpr const char *A_POST_FILTER  = "post_filter";
constexpr const char *A_RADIUS       = "radius";
constexpr const char *A_RIDGE_SELECT = "ridge_select";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_select_curvature_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Group 1: Cavities

  {
    node.set_current_group(G_CAVITIES);

    node.set_current_category("Cavity Parameters");
    add_float(node, A_RADIUS, "Selection Scale", 0.05f, 0.001f, 0.2f);
    add_bool(node, A_CONCAVE, "", "Bumps", "Holes", false);
    add_bool(node, A_POST_FILTER, "Enable Smoothing", true);

    setup_post_process_heightmap_attributes(
        node,
        {.add_mix = true, .remap_active_state = true});
  }

  // --- Group 2: Valley

  {
    node.set_current_group(G_VALLEY);

    node.set_current_category("Valley Parameters");
    add_float(node, A_RADIUS, "Radius", 0.05f, 0.001f, 0.5f);
    add_bool(node, A_RIDGE_SELECT, "Ridge Select", false);

    setup_post_process_heightmap_attributes(
        node,
        {.add_mix = false, .remap_active_state = true});
  }

  // --- Group 3: Blob (LoG)

  {
    node.set_current_group(G_BLOB_LOG);

    node.set_current_category("Blob Parameters");
    add_float(node, A_RADIUS, "Radius", 0.05f, 0.001f, 0.2f);

    setup_post_process_heightmap_attributes(
        node,
        {.add_mix = false, .remap_active_state = true});
  }

  // Reset active group to first
  node.set_current_group(G_CAVITIES);
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_select_curvature_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_in  = node.get_value_ref<hmap::VirtualArray>(P_IN);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_in || !p_out)
    return;

  const std::string group = node.get_meta_group().current_container_name().value_or(
      G_CAVITIES);

  // --- Group Dispatch

  if (group == G_CAVITIES)
  {
    int ir = node.val_pixel_radius(A_RADIUS);

    hmap::for_each_tile(
        {p_out, p_in},
        [&node, ir](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);
          *pa_out = hmap::gpu::select_cavities(*pa_in, ir, node.val<bool>(A_CONCAVE));

          if (node.val<bool>(A_POST_FILTER))
            hmap::laplace(*pa_out);
        },
        node.cfg().cm_gpu);

    p_out->smooth_overlap_buffers();
    post_process_heightmap(node, *p_out);
  }
  else if (group == G_VALLEY)
  {
    int ir = node.val_pixel_radius(A_RADIUS);

    hmap::for_each_tile(
        {p_out, p_in},
        [&node, ir](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);
          *pa_out = hmap::gpu::select_valley(*pa_in, ir, node.val<bool>(A_RIDGE_SELECT));
        },
        node.cfg().cm_gpu);

    p_out->smooth_overlap_buffers();
    post_apply_saturate_percentile(node, *p_out, 0.f, 0.95f);
    post_process_heightmap(node, *p_out);
  }
  else if (group == G_BLOB_LOG)
  {
    int ir = hmap::convert_length_to_pixel(node.val<float>(A_RADIUS), p_out->shape.x);

    hmap::for_each_tile(
        {p_out, p_in},
        [&node, &ir](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);
          *pa_out              = hmap::select_blob_log(*pa_in, ir);
        },
        node.cfg().cm_cpu);

    p_out->smooth_overlap_buffers();
    post_process_heightmap(node, *p_out);
  }
}

} // namespace hesiod
