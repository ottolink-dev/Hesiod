/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/filters.hpp"
#include "highmap/range.hpp"

#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_IN = "input";
constexpr const char *P_MASK = "mask";
constexpr const char *P_OUT = "output";

constexpr const char *A_GAIN = "gain";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_gain_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Attributes

  node.add_attr<FloatAttribute>(A_GAIN, "Gain", 2.f, 0.01f, 10.f);

  // --- Attribute(s) order

  node.set_attr_ordered_key(
      {"_GROUPBOX_BEGIN_Main Parameters", A_GAIN, "_GROUPBOX_END_"});

  setup_pre_process_mask_attributes(node);
  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_gain_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);
  auto *p_mask = node.get_value_ref<hmap::VirtualArray>(P_MASK);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_in)
    return;

  // --- Params

  const auto gain = node.get_attr<FloatAttribute>(A_GAIN);

  // --- Prepare mask

  std::shared_ptr<hmap::VirtualArray> sp_mask = pre_process_mask(node, p_mask, *p_in);

  // --- Compute

  float hmin = p_in->min(node.cfg().cm_cpu);
  float hmax = p_in->max(node.cfg().cm_cpu);

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

        hmap::remap(*pa_out, 0.f, 1.f, hmin, hmax);
        hmap::gain(*pa_out, gain, pa_mask);
        hmap::remap(*pa_out, hmin, hmax, 0.f, 1.f);
      },
      node.cfg().cm_cpu);

  // --- Post-process

  post_process_heightmap(node, *p_out, p_in);
}

} // namespace hesiod
