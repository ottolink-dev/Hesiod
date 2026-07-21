/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/erosion.hpp"

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

constexpr const char *P_Z_IN = "elevation_in";
constexpr const char *P_DEPTH_IN = "water_depth_in";
constexpr const char *P_MASK_IN = "mask";

constexpr const char *P_Z_OUT = "elevation";
constexpr const char *P_DEPTH_OUT = "water_depth";
constexpr const char *P_MASK_OUT = "water_mask";

constexpr const char *A_ADDITIONAL_DEPTH = "additional_depth";
constexpr const char *A_ITERATIONS = "iterations";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_coastal_erosion_diffusion_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_Z_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DEPTH_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_Z_OUT, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_DEPTH_OUT, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_MASK_OUT, CONFIG(node));

  // --- Attributes

  // clang-format off
  node.add_attr<FloatAttribute>(A_ADDITIONAL_DEPTH, "Additional Depth", 0.05f, 0.f, 0.2f);
  node.add_attr<IntAttribute>(A_ITERATIONS, "Iterations", 10, 0, INT_MAX);
  // clang-format on

  // --- Attribute(s) order

  node.set_attr_ordered_key({"_GROUPBOX_BEGIN_Main Parameters",
                             A_ADDITIONAL_DEPTH,
                             A_ITERATIONS,
                             "_GROUPBOX_END_"});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_coastal_erosion_diffusion_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_z = node.get_value_ref<hmap::VirtualArray>(P_Z_IN);
  auto *p_depth = node.get_value_ref<hmap::VirtualArray>(P_DEPTH_IN);
  auto *p_mask = node.get_value_ref<hmap::VirtualArray>(P_MASK_IN);

  auto *p_z_out = node.get_value_ref<hmap::VirtualArray>(P_Z_OUT);
  auto *p_depth_out = node.get_value_ref<hmap::VirtualArray>(P_DEPTH_OUT);
  auto *p_mask_out = node.get_value_ref<hmap::VirtualArray>(P_MASK_OUT);

  if (!p_z || !p_depth)
    return;

  // --- Params

  const auto additional_depth = node.get_attr<FloatAttribute>(A_ADDITIONAL_DEPTH);
  const auto iterations = node.get_attr<IntAttribute>(A_ITERATIONS);

  // --- Compute

  hmap::for_each_tile(
      {p_z, p_depth, p_mask},
      {p_z_out, p_depth_out, p_mask_out},
      [&](std::vector<const hmap::Array *> in,
          std::vector<hmap::Array *>       out,
          const hmap::TileRegion &)
      {
        auto [pa_z, pa_depth, pa_mask] = unpack<3>(in);

        auto [pa_z_out, pa_depth_out, pa_mask_out] = unpack<3>(out);

        *pa_z_out = *pa_z;
        *pa_depth_out = *pa_depth;

        hmap::coastal_erosion_diffusion(*pa_z_out,
                                        *pa_depth_out,
                                        additional_depth,
                                        iterations,
                                        pa_mask,
                                        pa_mask_out);
      },
      node.cfg().cm_cpu);
}

} // namespace hesiod
