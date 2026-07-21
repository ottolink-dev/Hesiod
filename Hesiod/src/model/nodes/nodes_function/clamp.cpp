/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
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
constexpr const char *P_OUT = "output";

constexpr const char *A_CLAMP = "clamp";
constexpr const char *A_SMOOTH_MIN = "smooth_min";
constexpr const char *A_K_MIN = "k_min";
constexpr const char *A_SMOOTH_MAX = "smooth_max";
constexpr const char *A_K_MAX = "k_max";
constexpr const char *A_REMAP = "remap";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_clamp_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Attributes

  node.add_attr<RangeAttribute>(A_CLAMP, "Clamp");
  node.add_attr<BoolAttribute>(A_SMOOTH_MIN, "Smooth Min", false);
  node.add_attr<FloatAttribute>(A_K_MIN, "K Min", 0.05f, 0.01f, 1.f);
  node.add_attr<BoolAttribute>(A_SMOOTH_MAX, "Smooth Max", false);
  node.add_attr<FloatAttribute>(A_K_MAX, "K Max", 0.05f, 0.01f, 1.f);
  node.add_attr<BoolAttribute>(A_REMAP, "Remap", false);

  // histogram binding
  setup_histogram_for_range_attribute(node, A_CLAMP, P_IN);

  // --- Attribute(s) order

  node.set_attr_ordered_key({"_GROUPBOX_BEGIN_Range",
                             A_CLAMP,
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Smoothing",
                             A_SMOOTH_MIN,
                             A_K_MIN,
                             A_SMOOTH_MAX,
                             A_K_MAX,
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Post",
                             A_REMAP,
                             "_GROUPBOX_END_"});

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_clamp_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_in)
    return;

  // --- Params

  const auto crange = node.get_attr<RangeAttribute>(A_CLAMP);
  const auto smooth_min = node.get_attr<BoolAttribute>(A_SMOOTH_MIN);
  const auto smooth_max = node.get_attr<BoolAttribute>(A_SMOOTH_MAX);
  const auto k_min = node.get_attr<FloatAttribute>(A_K_MIN);
  const auto k_max = node.get_attr<FloatAttribute>(A_K_MAX);
  const auto remap = node.get_attr<BoolAttribute>(A_REMAP);

  // --- Compute

  hmap::for_each_tile(
      {p_in},
      {p_out},
      [&](std::vector<const hmap::Array *> in,
          std::vector<hmap::Array *>       out,
          const hmap::TileRegion &)
      {
        auto [pa_in] = unpack<1>(in);
        auto [pa_out] = unpack<1>(out);

        *pa_out = *pa_in;

        // min clamp
        if (smooth_min)
          hmap::clamp_min_smooth(*pa_out, crange.x, k_min);
        else
          hmap::clamp_min(*pa_out, crange.x);

        // max clamp
        if (smooth_max)
          hmap::clamp_max_smooth(*pa_out, crange.y, k_max);
        else
          hmap::clamp_max(*pa_out, crange.y);
      },
      node.cfg().cm_cpu);

  // --- Post-process

  if (remap)
    p_out->remap(0.f, 1.f, node.cfg().cm_cpu);

  post_process_heightmap(node, *p_out, p_in);
}

} // namespace hesiod
