/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/filters.hpp"
#include "highmap/math/array.hpp"
#include "highmap/opencl/gpu_opencl.hpp"

#include "attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports
// -----------------------------------------------------------------------------

constexpr const char *P_IN = "input";
constexpr const char *P_REF = "reference";
constexpr const char *P_MASK = "mask";
constexpr const char *P_OUT = "output";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_equalize_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_REF);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Attributes

  // none...

  // --- Attributes order

  node.set_attr_ordered_key({});

  setup_pre_process_mask_attributes(node);
  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_equalize_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);
  auto *p_ref = node.get_value_ref<hmap::VirtualArray>(P_REF);
  auto *p_mask = node.get_value_ref<hmap::VirtualArray>(P_MASK);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_in)
    return;

  // --- Prepare mask

  std::shared_ptr<hmap::VirtualArray> sp_mask = pre_process_mask(node, p_mask, *p_in);

  // --- Compute

  if (p_ref)
  {
    hmap::for_each_tile(
        {p_in, p_ref, p_mask},
        {p_out},
        [&](std::vector<const hmap::Array *> in,
            std::vector<hmap::Array *>       out,
            const hmap::TileRegion &)
        {
          auto [pa_in, pa_ref, pa_mask] = unpack<3>(in);
          auto [pa_out] = unpack<1>(out);

          *pa_out = *pa_in;
          hmap::match_histogram(*pa_out, *pa_ref);

          // apply mask
          if (pa_mask)
            *pa_out = hmap::lerp(*pa_in, *pa_out, *pa_mask);
        },
        node.cfg().cm_single_array);
  }
  else
  {
    hmap::for_each_tile(
        {p_in, p_mask},
        {p_out},
        [&](std::vector<const hmap::Array *> in,
            std::vector<hmap::Array *>       out,
            const hmap::TileRegion &)
        {
          auto [pa_in, pa_mask] = unpack<2>(in);
          auto [pa_out] = unpack<1>(out);

          *pa_out = *pa_in;

          hmap::equalize(*pa_out, pa_mask);
        },
        node.cfg().cm_single_array);
  }

  // --- Post-process

  post_process_heightmap(node, *p_out, p_in);
}

} // namespace hesiod
