/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/primitives.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_DR  = "dr";
constexpr const char *P_OUT = "output";

constexpr const char *A_RADIUS        = "radius";
constexpr const char *A_SIGMA_INNER   = "sigma_inner";
constexpr const char *A_SIGMA_OUTER   = "sigma_outer";
constexpr const char *A_NOISE_R_AMP   = "noise_r_amp";
constexpr const char *A_Z_BOTTOM      = "z_bottom";
constexpr const char *A_NOISE_RATIO_Z = "noise_ratio_z";
constexpr const char *A_CENTER        = "center";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_caldera_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DR);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Attributes

  node.set_current_category("Geometry");
  add_float(node, A_RADIUS, "radius", 0.25f, 0.01f, 1.f);
  add_float(node, A_Z_BOTTOM, "z_bottom", 0.5f, 0.f, 1.f);
  add_xy(node, A_CENTER, "center");

  node.set_current_category("Profile");
  add_float(node, A_SIGMA_INNER, "sigma_inner", 0.05f, 0.f, 0.3f);
  add_float(node, A_SIGMA_OUTER, "sigma_outer", 0.1f, 0.f, 0.3f);

  node.set_current_category("Noise");
  add_float(node, A_NOISE_R_AMP, "noise_r_amp", 0.1f, 0.f, 0.3f);
  add_float(node, A_NOISE_RATIO_Z, "noise_ratio_z", 0.1f, 0.f, 1.f);

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = false, .remap_active_state = true});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_caldera_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_dr  = node.get_value_ref<hmap::VirtualArray>(P_DR);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_out)
    return;

  // --- Params

  const auto radius        = node.val<float>(A_RADIUS);
  const auto sigma_inner   = node.val<float>(A_SIGMA_INNER);
  const auto sigma_outer   = node.val<float>(A_SIGMA_OUTER);
  const auto noise_r_amp   = node.val<float>(A_NOISE_R_AMP);
  const auto z_bottom      = node.val<float>(A_Z_BOTTOM);
  const auto noise_ratio_z = node.val<float>(A_NOISE_RATIO_Z);
  const auto center        = node.val<glm::vec2>(A_CENTER);

  // --- Compute

  hmap::for_each_tile(
      {p_out, p_dr},
      [&](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &region)
      {
        hmap::Array *pa_out = p_arrays[0];
        hmap::Array *pa_dr  = p_arrays[1];

        *pa_out = hmap::caldera(region.shape,
                                radius,
                                sigma_inner,
                                sigma_outer,
                                z_bottom,
                                pa_dr,
                                noise_r_amp,
                                noise_ratio_z,
                                center,
                                region.bbox);
      },
      node.cfg().cm_cpu);

  // --- Post-process

  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
