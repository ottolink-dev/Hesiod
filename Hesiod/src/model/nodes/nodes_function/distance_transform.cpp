/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/filters.hpp"
#include "highmap/morphology.hpp"

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

constexpr const char *A_REVERSE_INPUT               = "reverse_input";
constexpr const char *A_THRESHOLD                   = "threshold";
constexpr const char *A_USE_DISTRIBUTED_COMPUTATION = "use_distributed_computation";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_distance_transform_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Attributes

  // clang-format off
  add_bool(node, A_REVERSE_INPUT, "reverse_input", false);
  add_float(node, A_THRESHOLD, "threshold", 0.1f, -1.f, 2.f);
  // clang-format on

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = true});

  // --- Advanced parameters

  node.set_current_category("Advanced");
  add_bool(node, A_USE_DISTRIBUTED_COMPUTATION, "Use Distributed Algorithm", false);
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_distance_transform_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_in  = node.get_value_ref<hmap::VirtualArray>(P_IN);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_in)
    return;

  // --- Params

  // clang-format off
  const auto reverse_input               = node.val<bool>(A_REVERSE_INPUT);
  const auto threshold                   = node.val<float>(A_THRESHOLD);
  const auto use_distributed_computation = node.val<bool>(A_USE_DISTRIBUTED_COMPUTATION);
  // clang-format on

  const auto cm = use_distributed_computation ? node.cfg().cm_cpu
                                              : node.cfg().cm_single_array;

  // --- Compute

  hmap::for_each_tile(
      {p_in},
      {p_out},
      [&](std::vector<const hmap::Array *> p_arrays_in,
          std::vector<hmap::Array *>       p_arrays_out,
          const hmap::TileRegion          &region,
          hmap::ICommunicator             &comm)
      {
        auto [pa_in]  = unpack<1>(p_arrays_in);
        auto [pa_out] = unpack<1>(p_arrays_out);

        *pa_out = *pa_in;

        make_binary(*pa_out, threshold);

        if (reverse_input)
          *pa_out = 1.f - *pa_out;

        *pa_out = hmap::distance_transform(*pa_out, false, region, comm);
      },
      cm);

  // --- Post-process

  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
