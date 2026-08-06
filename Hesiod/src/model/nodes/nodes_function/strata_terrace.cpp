/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/erosion.hpp"
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/primitives.hpp"
#include "highmap/range.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"
#include "hesiod/model/utils.hpp"

using namespace attr;

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_IN = "input";
constexpr const char *P_NOISE = "noise";
constexpr const char *P_MASK = "mask";
constexpr const char *P_OUT = "output";

constexpr const char *A_KZ = "kz";
constexpr const char *A_GAMMA = "gamma";
constexpr const char *A_SEED = "seed";
constexpr const char *A_LINEAR_GAMMA = "linear_gamma";
constexpr const char *A_GAMMA_NOISE_RATIO = "gamma_noise_ratio";
constexpr const char *A_SLOPE = "slope";
constexpr const char *A_ANGLE = "angle";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_strata_terrace_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_NOISE);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Attributes

  // clang-format off
  node.add_attr<FloatAttribute>(A_KZ, "Number of Strata", 8.f, 0.f, FLT_MAX);
  node.add_attr<FloatAttribute>(A_GAMMA, "Profile Gamma", 0.5f, 0.01f, 2.f);
  node.add_attr<SeedAttribute>(A_SEED, "Seed");
  node.add_attr<BoolAttribute>(A_LINEAR_GAMMA, "Use Linear Terrace Profile", false);
  node.add_attr<FloatAttribute>(A_GAMMA_NOISE_RATIO, "Gamma Noise Influence", 0.5f, 0.f, 1.f);
  node.add_attr<FloatAttribute>(A_SLOPE, "Slope", 0.f, 0.f, 10.f);
  node.add_attr<FloatAttribute>(A_ANGLE, "Angle", 0.f, -180.f, 180.f, "{:.1f}°");
  // clang-format on

  // --- Attribute(s) order

  node.set_attr_ordered_key({"_GROUPBOX_BEGIN_Strata Quantization",
                             A_KZ,
                             A_GAMMA,
                             A_LINEAR_GAMMA,
                             "_GROUPBOX_END_",
                             "_GROUPBOX_BEGIN_Orientation / Rotation",
                             A_SLOPE,
                             A_ANGLE,
                             "_GROUPBOX_END_",
                             "_GROUPBOX_BEGIN_Profile Variation",
                             A_SEED,
                             A_GAMMA_NOISE_RATIO,
                             "_GROUPBOX_END_"});

  setup_pre_process_mask_attributes(node);
  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_strata_terrace_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);
  auto *p_noise = node.get_value_ref<hmap::VirtualArray>(P_NOISE);
  auto *p_mask = node.get_value_ref<hmap::VirtualArray>(P_MASK);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_in || !p_out)
    return;

  // --- Params

  // clang-format off
  const auto kz                = node.get_attr<FloatAttribute>(A_KZ);
  const auto gamma             = node.get_attr<FloatAttribute>(A_GAMMA);
  const auto seed              = node.get_attr<SeedAttribute>(A_SEED);
  const auto linear_gamma      = node.get_attr<BoolAttribute>(A_LINEAR_GAMMA);
  const auto gamma_noise_ratio = node.get_attr<FloatAttribute>(A_GAMMA_NOISE_RATIO);
  const auto slope             = node.get_attr<FloatAttribute>(A_SLOPE);
  const auto angle             = node.get_attr<FloatAttribute>(A_ANGLE);
  // clang-format on

  // --- Prepare mask

  std::shared_ptr<hmap::VirtualArray> sp_mask = pre_process_mask(node, p_mask, *p_in);

  // --- Compute

  hmap::for_each_tile(
      {p_in, p_noise, p_mask},
      {p_out},
      [&](std::vector<const hmap::Array *> in,
          std::vector<hmap::Array *>       out,
          const hmap::TileRegion &)
      {
        auto [pa_in, pa_noise, pa_mask] = unpack<3>(in);
        auto [pa_out] = unpack<1>(out);

        *pa_out = *pa_in;

        hmap::gpu::strata_terrace(*pa_out,
                                  gamma,
                                  seed,
                                  pa_mask,
                                  kz,
                                  linear_gamma,
                                  gamma_noise_ratio,
                                  slope,
                                  angle,
                                  pa_noise);
      },
      node.cfg().cm_gpu);

  // --- Post-process

  p_out->smooth_overlap_buffers();
  post_process_heightmap(node, *p_out, p_in);
}

} // namespace hesiod
