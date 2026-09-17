/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
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

constexpr const char *P_DR   = "dr";
constexpr const char *P_ENV  = "envelope";
constexpr const char *P_MASK = "mask";
constexpr const char *P_OUT  = "output";

constexpr const char *A_ANGLE         = "angle";
constexpr const char *A_CENTER        = "center";
constexpr const char *A_CENTER_DEPTH  = "center_depth";
constexpr const char *A_EXIT_DEPTH    = "exit_depth";
constexpr const char *A_EXIT_WIDTH    = "exit_width";
constexpr const char *A_OUTER_FALLOFF = "outer_falloff";
constexpr const char *A_RADIUS        = "radius";
constexpr const char *A_RIDGE_HEIGHT  = "ridge_height";
constexpr const char *A_RIDGE_WIDTH   = "ridge_width";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_circus_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DR);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ENV);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_MASK, CONFIG(node));

  // --- Attributes

  node.set_current_category("Geometry");
  add_float(node, A_RADIUS, "Radius", 0.4f, 0.01f, 2.f);
  add_angle(node, A_ANGLE, "Angle", 0.f, -180.f, 180.f);
  add_xy(node, A_CENTER, "Center");

  node.set_current_category("Basin");
  add_float(node, A_EXIT_WIDTH, "Exit Width", 0.35f, 0.01f, 2.f);
  add_float(node, A_EXIT_DEPTH, "Exit Depth", 0.f, 0.f, 1.f);
  add_float(node, A_CENTER_DEPTH, "Center Depth", 0.1f, 0.f, 1.f);

  node.set_current_category("Ridge");
  add_float(node, A_RIDGE_HEIGHT, "Ridge Height", 1.f, 0.f, 2.f);
  add_float(node, A_RIDGE_WIDTH, "Ridge Width", 0.15f, 0.01f, 1.f);
  add_float(node, A_OUTER_FALLOFF, "Outer Falloff", 2.f, 0.1f, 10.f);

  // --- Noise and post process

  setup_default_noise(
      node,
      {.noise_amp = 0.15f, .kw = 4.f, .smoothness = 0.3f, .noise_type = "Perlin"});
  setup_post_process_heightmap_attributes(
      node,
      {.add_mix = false, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_circus_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_dr   = node.get_value_ref<hmap::VirtualArray>(P_DR);
  auto *p_env  = node.get_value_ref<hmap::VirtualArray>(P_ENV);
  auto *p_out  = node.get_value_ref<hmap::VirtualArray>(P_OUT);
  auto *p_mask = node.get_value_ref<hmap::VirtualArray>(P_MASK);

  if (!p_out)
    return;

  // --- Params

  const auto radius        = node.val<float>(A_RADIUS);
  const auto angle         = node.val<float>(A_ANGLE);
  const auto exit_width    = node.val<float>(A_EXIT_WIDTH);
  const auto exit_depth    = node.val<float>(A_EXIT_DEPTH);
  const auto center_depth  = node.val<float>(A_CENTER_DEPTH);
  const auto ridge_height  = node.val<float>(A_RIDGE_HEIGHT);
  const auto ridge_width   = node.val<float>(A_RIDGE_WIDTH);
  const auto outer_falloff = node.val<float>(A_OUTER_FALLOFF);
  const auto center        = node.val<glm::vec2>(A_CENTER);

  // --- Resolve default noise

  hmap::VirtualArray noise_default(CONFIG(node));
  generate_noise(node, p_dr, noise_default);

  // --- Compute

  hmap::for_each_tile(
      {p_dr},
      {p_out, p_mask},
      [&](std::vector<const hmap::Array *> in,
          std::vector<hmap::Array *>       out,
          const hmap::TileRegion          &region)
      {
        auto [pa_dr]           = unpack<1>(in);
        auto [pa_out, pa_mask] = unpack<2>(out);

        *pa_out = hmap::circus(region.shape,
                               radius,
                               angle,
                               exit_width,
                               exit_depth,
                               center_depth,
                               ridge_height,
                               ridge_width,
                               outer_falloff,
                               pa_dr,
                               center,
                               region.bbox,
                               pa_mask);
      },
      node.cfg().cm_cpu);

  // --- Post-process

  post_apply_enveloppe(node, *p_out, p_env);
  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
