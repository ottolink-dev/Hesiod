/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/filters.hpp"
#include "highmap/opencl/gpu_opencl.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_IN    = "input";
constexpr const char *P_ANGLE = "angle";
constexpr const char *P_MASK  = "mask";
constexpr const char *P_OUT   = "output";

constexpr const char *A_RADIUS = "radius";
constexpr const char *A_ANGLE  = "angle";
constexpr const char *A_STEPS  = "steps";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

namespace hesiod
{

void setup_directional_blur_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ANGLE);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>(A_RADIUS, "radius", 0.05f, 0.f, 0.2f);
  node.add_attr<FloatAttribute>(A_ANGLE, "angle", 0.f, -180.f, 180.f);
  node.add_attr<IntAttribute>(A_STEPS, "steps", 16, 1, 128);

  // attribute(s) order
  node.set_attr_ordered_key({A_RADIUS, A_ANGLE, A_STEPS});

  setup_pre_process_mask_attributes(node);
  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_directional_blur_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_in    = node.get_value_ref<hmap::VirtualArray>(P_IN);
  auto *p_angle = node.get_value_ref<hmap::VirtualArray>(P_ANGLE);
  auto *p_out   = node.get_value_ref<hmap::VirtualArray>(P_OUT);
  auto *p_mask  = node.get_value_ref<hmap::VirtualArray>(P_MASK);

  if (!p_in)
    return;

  // --- Params

  // clang-format off
  const auto radius = node.get_attr<FloatAttribute>(A_RADIUS);
  const auto angle  = node.get_attr<FloatAttribute>(A_ANGLE);
  const auto steps  = node.get_attr<IntAttribute>(A_STEPS);
  // clang-format on

  float r_pixels = radius * p_out->shape.x;

  // --- Resolve mask

  std::shared_ptr<hmap::VirtualArray> sp_mask = pre_process_mask(node, p_mask, *p_in);

  // --- Compute

  hmap::for_each_tile(
      {p_out, p_in, p_angle, p_mask},
      [&](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
      {
        auto [pa_out, pa_in, pa_angle, pa_mask] = unpack<4>(p_arrays);

        // resolve angle field
        hmap::Array angle_deg(pa_in->shape, angle);

        if (pa_angle)
          angle_deg += (*pa_angle) * 180.f / M_PI;

        // apply filter
        *pa_out = *pa_in;
        hmap::gpu::directional_blur(*pa_out, r_pixels, angle_deg, pa_mask, steps);
      },
      node.cfg().cm_gpu);

  p_out->smooth_overlap_buffers();

  // post-process
  post_process_heightmap(node, *p_out, p_in);
}

} // namespace hesiod
