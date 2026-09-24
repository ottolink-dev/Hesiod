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

constexpr const char *P_DX       = "dx";
constexpr const char *P_DY       = "dy";
constexpr const char *P_ENVELOPE = "envelope";
constexpr const char *P_OUT      = "output";

constexpr const char *A_C00 = "c00";
constexpr const char *A_C10 = "c10";
constexpr const char *A_C01 = "c01";
constexpr const char *A_C11 = "c11";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_quad_surface_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DX);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DY);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ENVELOPE);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Attributes

  // clang-format off
  add_float(node, A_C00, "c00 (Bottom-Left)", 0.5f, -1.f, 1.f);
  add_float(node, A_C10, "c10 (Bottom-Right)", 0.f, -1.f, 1.f);
  add_float(node, A_C01, "c01 (Top-Left)", 0.f, -1.f, 1.f);
  add_float(node, A_C11, "c11 (Top-Right)", 0.f, -1.f, 1.f);
  // clang-format on

  // --- Attribute(s) order

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = false, .remap_active_state = true});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_quad_surface_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_dx  = node.get_value_ref<hmap::VirtualArray>(P_DX);
  auto *p_dy  = node.get_value_ref<hmap::VirtualArray>(P_DY);
  auto *p_env = node.get_value_ref<hmap::VirtualArray>(P_ENVELOPE);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_out)
    return;

  // --- Params

  const auto c00 = node.val<float>(A_C00);
  const auto c10 = node.val<float>(A_C10);
  const auto c01 = node.val<float>(A_C01);
  const auto c11 = node.val<float>(A_C11);

  // --- Compute

  hmap::for_each_tile(
      {p_dx, p_dy},
      {p_out},
      [&](std::vector<const hmap::Array *> in,
          std::vector<hmap::Array *>       out,
          const hmap::TileRegion          &region)
      {
        auto [pa_dx, pa_dy] = unpack<2>(in);
        auto [pa_out]       = unpack<1>(out);

        *pa_out = hmap::quad_surface(region.shape,
                                     c00,
                                     c10,
                                     c01,
                                     c11,
                                     nullptr,
                                     pa_dx,
                                     pa_dy,
                                     region.bbox);
      },
      node.cfg().cm_cpu);

  // --- Post-process

  post_apply_enveloppe(node, *p_out, p_env);
  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
