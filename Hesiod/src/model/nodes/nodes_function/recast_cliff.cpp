/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/filters.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_ANGLE      = "angle";
constexpr const char *P_CLIFF_MASK = "cliff_mask";
constexpr const char *P_IN         = "input";
constexpr const char *P_MASK       = "mask";
constexpr const char *P_OUT        = "output";

constexpr const char *G_ISOTROPIC   = "Isotropic";
constexpr const char *G_DIRECTIONAL = "Directional";

constexpr const char *A_AMPLITUDE    = "amplitude";
constexpr const char *A_ANGLE        = "angle";
constexpr const char *A_GAIN         = "gain";
constexpr const char *A_ITERATIONS   = "iterations";
constexpr const char *A_RADIUS       = "radius";
constexpr const char *A_TALUS_GLOBAL = "talus_global";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_recast_cliff_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ANGLE);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_CLIFF_MASK, CONFIG(node));

  // --- Group 1: Isotropic

  {
    node.set_current_group(G_ISOTROPIC);

    node.set_current_category("Main Parameters");
    add_float(node, A_TALUS_GLOBAL, "talus_global", 2.f, 0.f, FLT_MAX);
    add_float(node, A_RADIUS, "radius", 0.f, 0.f, 0.5f);
    add_float(node, A_AMPLITUDE, "amplitude", 1.f, 0.f, 2.f);
    add_float(node, A_GAIN, "gain", 4.f, 0.01f, 8.f);
    add_int(node, A_ITERATIONS, "iterations", 500, 1, 1000);

    setup_pre_process_mask_attributes(node);
    setup_post_process_heightmap_attributes(
        node,
        {.add_mix = true, .remap_active_state = false});
  }

  // --- Group 2: Directional

  {
    node.set_current_group(G_DIRECTIONAL);

    node.set_current_category("Main Parameters");
    add_angle(node, A_ANGLE, "angle", 45.f, -180.f, 180.f);
    add_float(node, A_TALUS_GLOBAL, "talus_global", 1.f, 0.f, 5.f);
    add_float(node, A_RADIUS, "radius", 0.1f, 0.f, 0.5f);
    add_float(node, A_AMPLITUDE, "amplitude", 0.1f, 0.f, 1.f);
    add_float(node, A_GAIN, "gain", 2.f, 0.01f, 10.f);
    add_int(node, A_ITERATIONS, "iterations", 500, 1, 1000);

    setup_pre_process_mask_attributes(node);
    setup_post_process_heightmap_attributes(
        node,
        {.add_mix = true, .remap_active_state = false});
  }

  // Reset active group to first
  node.set_current_group(G_ISOTROPIC);
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_recast_cliff_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_in         = node.get_value_ref<hmap::VirtualArray>(P_IN);
  auto *p_mask       = node.get_value_ref<hmap::VirtualArray>(P_MASK);
  auto *p_angle      = node.get_value_ref<hmap::VirtualArray>(P_ANGLE);
  auto *p_out        = node.get_value_ref<hmap::VirtualArray>(P_OUT);
  auto *p_cliff_mask = node.get_value_ref<hmap::VirtualArray>(P_CLIFF_MASK);

  if (!p_in)
    return;

  const std::string group = node.get_meta_group().current_container_name().value_or(
      G_ISOTROPIC);

  // --- Params

  const auto talus      = node.val<float>(A_TALUS_GLOBAL) / (float)p_out->shape.x;
  const auto ir         = node.val_pixel_radius(A_RADIUS, 0);
  const auto amplitude  = node.val<float>(A_AMPLITUDE);
  const auto gain       = node.val<float>(A_GAIN);
  const auto iterations = node.val<int>(A_ITERATIONS);
  const auto angle      = (group == G_DIRECTIONAL) ? node.val<float>(A_ANGLE) : 0.f;

  // --- Prepare mask

  std::shared_ptr<hmap::VirtualArray> sp_mask = pre_process_mask(node, p_mask, *p_in);

  // --- Compute

  hmap::for_each_tile(
      {p_in, p_mask, p_angle},
      {p_out, p_cliff_mask},
      [&](std::vector<const hmap::Array *> in,
          std::vector<hmap::Array *>       out,
          const hmap::TileRegion &)
      {
        auto [pa_in, pa_mask, pa_angle] = unpack<3>(in);
        auto [pa_out, pa_cliff_mask]    = unpack<2>(out);

        *pa_out = *pa_in;

        if (group == G_DIRECTIONAL)
        {
          if (pa_angle)
          {
            hmap::Array angle_deg(pa_in->shape, angle);
            angle_deg += (*pa_angle) * (180.f / static_cast<float>(M_PI));
            hmap::gpu::recast_cliff_directional(*pa_out,
                                                talus,
                                                ir,
                                                amplitude,
                                                angle_deg,
                                                pa_mask,
                                                gain,
                                                iterations,
                                                pa_cliff_mask);
          }
          else
          {
            hmap::gpu::recast_cliff_directional(*pa_out,
                                                talus,
                                                ir,
                                                amplitude,
                                                angle,
                                                pa_mask,
                                                gain,
                                                iterations,
                                                pa_cliff_mask);
          }
        }
        else
        {
          hmap::gpu::recast_cliff(*pa_out,
                                  talus,
                                  ir,
                                  amplitude,
                                  pa_mask,
                                  gain,
                                  iterations,
                                  pa_cliff_mask);
        }
      },
      node.cfg().cm_gpu);

  p_out->smooth_overlap_buffers();

  // input amplitude preservation
  p_out->remap(p_in->min(node.cfg().cm_cpu),
               p_in->max(node.cfg().cm_cpu),
               node.cfg().cm_cpu);

  if (p_cliff_mask)
    p_cliff_mask->smooth_overlap_buffers();

  // --- Post-process

  post_process_heightmap(node, *p_out, p_in);
}

} // namespace hesiod
