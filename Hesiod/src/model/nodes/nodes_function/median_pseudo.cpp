/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/filters.hpp"

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

constexpr const char *A_RADIUS         = "radius";
constexpr const char *A_MIN_MAX_KERNEL = "min_max_kernel";

void setup_median_pseudo_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  add_float(node, A_RADIUS, "radius", 0.1f, 0.f, 0.5f);

  setup_post_process_heightmap_attributes(
      node,
      {.add_mix = false, .remap_active_state = false});

  node.set_current_category("Advanced");
  add_enum(node,
           A_MIN_MAX_KERNEL,
           "Kernel Type",
           enum_mappings.min_max_kernel_map,
           "Octagon");
}

void compute_median_pseudo_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);

  if (p_in)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

    const auto kernel_type = node.val_enum<hmap::MinMaxKernel>(A_MIN_MAX_KERNEL);
    int        ir          = node.val_pixel_radius(A_RADIUS, 0);

    hmap::for_each_tile(
        {p_out, p_in},
        [&node, ir, kernel_type](std::vector<hmap::Array *> p_arrays,
                                 const hmap::TileRegion &)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);
          *pa_out              = hmap::gpu::median_pseudo(*pa_in, ir, kernel_type);
        },
        node.cfg().cm_gpu);

    p_out->smooth_overlap_buffers();

    // post-process
    post_process_heightmap(node, *p_out);
  }
}

} // namespace hesiod
