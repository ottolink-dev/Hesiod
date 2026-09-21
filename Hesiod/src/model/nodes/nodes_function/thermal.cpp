/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/erosion.hpp"
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/primitives.hpp"

#include "highmap/dbg/timer.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_IN         = "input";
constexpr const char *P_MASK       = "mask";
constexpr const char *P_OUT        = "output";
constexpr const char *P_DEPOSITION = "deposition";

constexpr const char *A_TYPE        = "type";
constexpr const char *A_TALUS       = "talus_global";
constexpr const char *A_DURATION    = "duration";
constexpr const char *A_SCALE_TALUS = "scale_talus_with_elevation";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_thermal_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_DEPOSITION, CONFIG(node));

  // --- Attributes

  std::vector<std::string> choices =
      {"Standard", "Linear", "Bedrock", "Olsen", "Ridge", "Schott", "Inflate"};

  // clang-format off
  add_choice(node, A_TYPE, "", choices);
  add_float(node, A_TALUS, "Slope", 1.f, 0.f, FLT_MAX);
  add_float(node, A_DURATION, "Duration", 0.3f, 0.05f, 6.f);
  add_bool(node, A_SCALE_TALUS, "Scale with Elevation", false);
  // clang-format on

  // --- Attribute(s) order

  setup_pre_process_mask_attributes(node);
  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_thermal_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_in         = node.get_value_ref<hmap::VirtualArray>(P_IN);
  auto *p_mask       = node.get_value_ref<hmap::VirtualArray>(P_MASK);
  auto *p_out        = node.get_value_ref<hmap::VirtualArray>(P_OUT);
  auto *p_deposition = node.get_value_ref<hmap::VirtualArray>(P_DEPOSITION);

  if (!p_in)
    return;

  // --- Prepare mask

  std::shared_ptr<hmap::VirtualArray> sp_mask = pre_process_mask(node, p_mask, *p_in);

  // --- Params

  // clang-format off
  const auto type = node.val<std::string>(A_TYPE);
  const auto talus_global = node.val<float>(A_TALUS);
  const auto duration = node.val<float>(A_DURATION);
  const auto scale_talus = node.val<bool>(A_SCALE_TALUS);
  // clang-format on

  const float talus      = talus_global / float(p_out->shape.x);
  const int   iterations = int(duration * p_out->shape.x);

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
      {p_in, p_mask, &talus_map},
      {p_out, p_deposition},
      [&](std::vector<const hmap::Array *> in,
          std::vector<hmap::Array *>       out,
          const hmap::TileRegion &)
      {
        auto [pa_in, pa_mask, pa_talus_map] = unpack<3>(in);
        auto [pa_out, pa_deposition]        = unpack<2>(out);

        *pa_out = *pa_in;

        if (type == "Standard")
        {
          hmap::gpu::thermal(*pa_out,
                             pa_mask,
                             *pa_talus_map,
                             iterations,
                             nullptr,
                             pa_deposition);
        }
        else if (type == "Ridge")
        {
          hmap::gpu::thermal_ridge(*pa_out,
                                   pa_mask,
                                   *pa_talus_map,
                                   iterations,
                                   pa_deposition);
        }
        else if (type == "Linear")
        {
          const int iterations_half = int(0.5f * iterations);

          hmap::gpu::thermal(*pa_out,
                             pa_mask,
                             *pa_talus_map,
                             iterations_half,
                             nullptr,
                             nullptr);

          hmap::gpu::thermal_ridge(*pa_out,
                                   pa_mask,
                                   *pa_talus_map,
                                   iterations_half,
                                   pa_deposition);
        }
        else if (type == "Bedrock")
        {
          hmap::gpu::thermal_auto_bedrock(*pa_out,
                                          pa_mask,
                                          *pa_talus_map,
                                          iterations,
                                          pa_deposition);
        }
        else if (type == "Olsen")
        {
          hmap::gpu::thermal_olsen(*pa_out, pa_mask, *pa_talus_map, iterations);

          *pa_deposition = *pa_out - *pa_in;
        }
        else if (type == "Schott")
        {
          hmap::gpu::thermal_schott(*pa_out, pa_mask, *pa_talus_map, iterations);

          *pa_deposition = *pa_out - *pa_in;
        }
        else if (type == "Inflate")
        {
          hmap::gpu::thermal_inflate(*pa_out, pa_mask, *pa_talus_map, iterations);

          *pa_deposition = *pa_out - *pa_in;
        }
      },
      node.cfg().cm_gpu);

  // --- Post-process

  post_process_heightmap(node, *p_out, p_in);

  if (p_deposition)
  {
    p_deposition->smooth_overlap_buffers();
    p_deposition->remap(0.f, 1.f, node.cfg().cm_cpu);
  }
}

} // namespace hesiod
