/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/primitives.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_OUT = "output";

constexpr const char *A_NOISE_RATIO                = "noise_ratio";
constexpr const char *A_SCALE                      = "scale";
constexpr const char *A_SEED                       = "seed";
constexpr const char *A_SEEDING_OUTER_RADIUS_RATIO = "seeding_outer_radius_ratio";
constexpr const char *A_SEEDING_RADIUS             = "seeding_radius";
constexpr const char *A_SLOPE                      = "slope";

void setup_diffusion_limited_aggregation_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  add_seed(node, A_SEED, "Seed");
  add_float(node, A_SCALE, "scale", 0.01f, 0.005f, 0.1f);
  add_float(node, A_SEEDING_RADIUS, "seeding_radius", 0.4f, 0.1f, 0.5f);
  add_float(node,
            A_SEEDING_OUTER_RADIUS_RATIO,
            "seeding_outer_radius_ratio",
            0.2f,
            0.01f,
            0.5f);
  add_float(node, A_SLOPE, "slope", 8.f, 0.1f, FLT_MAX);
  add_float(node, A_NOISE_RATIO, "noise_ratio", 0.2f, 0.f, 1.f);

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = false, .remap_active_state = true});
}

void compute_diffusion_limited_aggregation_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  hmap::Array array = hmap::diffusion_limited_aggregation(
      node.cfg().shape,
      node.val<float>(A_SCALE),
      node.val<int>(A_SEED),
      node.val<float>(A_SEEDING_RADIUS),
      node.val<float>(A_SEEDING_OUTER_RADIUS_RATIO),
      node.val<float>(A_SLOPE),
      node.val<float>(A_NOISE_RATIO));

  p_out->from_array(array, node.cfg().cm_cpu);

  // post-process
  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
