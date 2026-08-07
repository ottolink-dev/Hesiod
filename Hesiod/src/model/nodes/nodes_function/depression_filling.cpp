/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/erosion.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_IN       = "input";
constexpr const char *P_OUT      = "output";
constexpr const char *P_FILL_MAP = "fill map";

constexpr const char *A_REMAP_FILL_MAP = "remap fill map";
constexpr const char *A_SMOOTHING      = "smoothing";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_depression_filling_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_FILL_MAP, CONFIG(node));

  // --- Attributes

  // clang-format off
  node.add_attr<BoolAttribute>(A_SMOOTHING, "Smoothing", false);
  node.add_attr<BoolAttribute>(A_REMAP_FILL_MAP, "Remap Fill Map", true);
  // clang-format on

  // --- Attribute(s) order

  node.set_attr_ordered_key({"_GROUPBOX_BEGIN_Main Parameters",
                             A_SMOOTHING,
                             A_REMAP_FILL_MAP,
                             "_GROUPBOX_END_"});

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_depression_filling_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_in       = node.get_value_ref<hmap::VirtualArray>(P_IN);
  auto *p_out      = node.get_value_ref<hmap::VirtualArray>(P_OUT);
  auto *p_fill_map = node.get_value_ref<hmap::VirtualArray>(P_FILL_MAP);

  if (!p_in || !p_out || !p_fill_map)
    return;

  // --- Params

  // clang-format off
  const auto smoothing      = node.get_attr<BoolAttribute>(A_SMOOTHING);
  const auto remap_fill_map = node.get_attr<BoolAttribute>(A_REMAP_FILL_MAP);
  // clang-format on

  // --- Compute

  hmap::for_each_tile(
      {p_out, p_in, p_fill_map},
      [&](std::vector<hmap::Array *> arrays, const hmap::TileRegion &)
      {
        auto [pa_out, pa_in, pa_fill_map] = unpack<3>(arrays);

        *pa_out = *pa_in;

        hmap::depression_filling_priority_flood(*pa_out, smoothing);

        *pa_fill_map = *pa_out - *pa_in;
      },
      node.cfg().cm_single_array); // forced, not tileable

  // --- Post-process

  post_process_heightmap(node, *p_out, p_in);

  if (remap_fill_map)
    p_fill_map->remap(0.f, 1.f, node.cfg().cm_cpu);
}

} // namespace hesiod
