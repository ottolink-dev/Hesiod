/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/hydrology/hydrology.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_ELEVATION   = "elevation";
constexpr const char *P_WATER_DEPTH = "water_depth";
constexpr const char *P_WATER_MASK  = "water_mask";

constexpr const char *A_ITERATIONS     = "iterations";
constexpr const char *A_MASK_THRESHOLD = "mask_threshold";
constexpr const char *A_OMEGA          = "omega";
constexpr const char *A_TOLERANCE      = "tolerance";

void setup_water_depth_from_mask_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ELEVATION);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_WATER_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_WATER_DEPTH, CONFIG(node));

  // attribute(s)
  add_float(node,
            A_MASK_THRESHOLD,
            "Mask Activation Threshold",
            0.01f,
            0.f,
            1.f,
            "{:.3f}");
  add_float(node,
            A_TOLERANCE,
            "Convergence Tolerance",
            1e-6f,
            1e-8f,
            1e-4f,
            "{:.3e}",
            true);
  add_int(node, A_ITERATIONS, "Max Iterations", 500, 1, INT_MAX);
  add_float(node, A_OMEGA, "Relaxation Factor", 1.8f, 1e-3f, 1.9f);
}

void compute_water_depth_from_mask_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_z    = node.get_value_ref<hmap::VirtualArray>(P_ELEVATION);
  hmap::VirtualArray *p_mask = node.get_value_ref<hmap::VirtualArray>(P_WATER_MASK);

  if (p_z && p_mask)
  {
    hmap::VirtualArray *p_depth = node.get_value_ref<hmap::VirtualArray>(P_WATER_DEPTH);

    hmap::for_each_tile(
        {p_depth, p_z, p_mask},
        [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          hmap::Array *pa_depth = p_arrays[0];
          hmap::Array *pa_z     = p_arrays[1];
          hmap::Array *pa_mask  = p_arrays[2];

          *pa_depth = hmap::water_depth_from_mask(*pa_z,
                                                  *pa_mask,
                                                  node.val<float>(A_MASK_THRESHOLD),
                                                  node.val<int>(A_ITERATIONS),
                                                  node.val<float>(A_TOLERANCE),
                                                  node.val<float>(A_OMEGA));
        },
        node.cfg().cm_cpu);

    p_depth->smooth_overlap_buffers();
  }
}

} // namespace hesiod
