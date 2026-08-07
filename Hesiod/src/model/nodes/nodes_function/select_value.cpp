/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/math.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_IN  = "input";
constexpr const char *P_OUT = "output";

constexpr const char *A_VALUE     = "value";
constexpr const char *A_WIDTH     = "width";
constexpr const char *A_SMOOTHING = "smoothing";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_select_value_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Attributes

  // clang-format off
  add_float(node, A_VALUE, "Value", 0.5f, -1.f, 2.f);
  add_float(node, A_WIDTH, "Width", 0.1f, 0.f, 0.3f);
  add_float(node, A_SMOOTHING, "Smoothing", 0.02f, 0.001f, 1.f);
  // clang-format on

  // --- Attribute(s) order

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = false, .remap_active_state = true});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_select_value_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_in  = node.get_value_ref<hmap::VirtualArray>(P_IN);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_in || !p_out)
    return;

  // --- Params

  // clang-format off
  const auto value     = node.val<float>(A_VALUE);
  const auto width     = node.val<float>(A_WIDTH);
  const auto smoothing = node.val<float>(A_SMOOTHING);
  // clang-format on

  const float xmin = value - 0.5f * width;
  const float xmax = value + 0.5f * width;

  // --- Compute

  hmap::for_each_tile(
      {p_in},
      {p_out},
      [xmin, xmax, smoothing](std::vector<const hmap::Array *> in,
                              std::vector<hmap::Array *>       out,
                              const hmap::TileRegion &)
      {
        auto [pa_in]  = unpack<1>(in);
        auto [pa_out] = unpack<1>(out);

        *pa_out = hmap::sigmoid(*pa_in, smoothing, 0.f, 1.f, xmin);
        *pa_out *= 1.f - hmap::sigmoid(*pa_in, smoothing, 0.f, 1.f, xmax);
      },
      node.cfg().cm_cpu);

  // --- Post-process

  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
