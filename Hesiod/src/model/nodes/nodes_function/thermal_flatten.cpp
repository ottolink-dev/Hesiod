/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/erosion.hpp"
#include "highmap/opencl/gpu_opencl.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_IN   = "input";
constexpr const char *P_MASK = "mask";
constexpr const char *P_OUT  = "output";

constexpr const char *A_TALUS_GLOBAL = "talus_global";
constexpr const char *A_DURATION     = "duration";
constexpr const char *A_SCALE_TALUS  = "scale_talus_with_elevation";
constexpr const char *A_SIGMA_INF    = "sigma_inf";
constexpr const char *A_SIGMA_SUP    = "sigma_sup";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_thermal_flatten_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Attributes

  // clang-format off
  add_float(node, A_TALUS_GLOBAL, "talus_global", 1.f, 0.f, FLT_MAX);
  add_float(node, A_DURATION, "Duration", 0.3f, 0.05f, 6.f);
  add_bool(node, A_SCALE_TALUS, "scale_talus_with_elevation", false);
  add_float(node, A_SIGMA_INF, "sigma_inf", 0.5f, 0.f, 0.5f);
  add_float(node, A_SIGMA_SUP, "sigma_sup", 0.25f, 0.f, 0.5f);
  // clang-format on

  // --- Attribute(s) order
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_thermal_flatten_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_in   = node.get_value_ref<hmap::VirtualArray>(P_IN);
  auto *p_mask = node.get_value_ref<hmap::VirtualArray>(P_MASK);
  auto *p_out  = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_in)
    return;

  // --- Params

  // clang-format off
  const auto talus       = node.val<float>(A_TALUS_GLOBAL) / float(p_out->shape.x);
  const auto duration    = node.val<float>(A_DURATION);
  const auto scale_talus = node.val<bool>(A_SCALE_TALUS);
  const auto sigma_inf   = node.val<float>(A_SIGMA_INF);
  const auto sigma_sup   = node.val<float>(A_SIGMA_SUP);
  // clang-format on

  const int iterations = int(duration * p_out->shape.x);

  // --- Talus map

  hmap::VirtualArray talus_map(CONFIG(node));

  talus_map.fill(talus, node.cfg().cm_cpu);

  if (scale_talus)
  {
    talus_map.copy_from(*p_in, node.cfg().cm_cpu);
    talus_map.remap(talus / 100.f, talus, node.cfg().cm_cpu);
  }

  // --- Compute

  hmap::for_each_tile(
      {p_in, &talus_map, p_mask},
      {p_out},
      [&](std::vector<const hmap::Array *> in,
          std::vector<hmap::Array *>       out,
          const hmap::TileRegion &)
      {
        auto [pa_in, pa_talus_map, pa_mask] = unpack<3>(in);
        auto [pa_out]                       = unpack<1>(out);

        *pa_out = *pa_in;

        hmap::gpu::thermal_flatten(*pa_out,
                                   pa_mask,
                                   *pa_talus_map,
                                   iterations,
                                   sigma_inf,
                                   sigma_sup);
      },
      node.cfg().cm_gpu);

  // --- Post-process

  p_out->smooth_overlap_buffers();
}

} // namespace hesiod
