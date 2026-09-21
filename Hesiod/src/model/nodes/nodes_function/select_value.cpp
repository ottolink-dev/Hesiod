/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/math.hpp"
#include "highmap/selector.hpp"

#include "hesiod/model/nodes/attributes.hpp"

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

constexpr const char *G_INTERVAL  = "Interval";
constexpr const char *G_THRESHOLD = "Threshold";
constexpr const char *G_TARGET    = "Target";
constexpr const char *G_MIDRANGE  = "Midrange";

constexpr const char *A_VALUE1    = "value1";
constexpr const char *A_VALUE2    = "value2";
constexpr const char *A_WIDTH     = "width";
constexpr const char *A_X0        = "x0";
constexpr const char *A_VALUE     = "value";
constexpr const char *A_SMOOTHING = "smoothing";
constexpr const char *A_GAIN      = "gain";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_select_value_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Group 1: Interval

  {
    node.set_current_group(G_INTERVAL);

    node.set_current_category("Interval Parameters");
    add_float(node, A_VALUE1, "Lower Bound", 0.f, -0.5f, 1.5f);
    add_float(node, A_VALUE2, "Upper Bound", 0.5f, -0.5f, 1.5f);
    add_float(node, A_WIDTH, "Width", 0.1f, 0.f, 0.3f);

    setup_post_process_heightmap_attributes(
        node,
        {.add_mix = false, .remap_active_state = true});
  }

  // --- Group 2: Threshold

  {
    node.set_current_group(G_THRESHOLD);

    node.set_current_category("Threshold Parameters");
    add_float(node, A_X0, "Value", 0.5f, -1.f, 2.f);
    add_float(node, A_WIDTH, "Tolerance", 0.1f, 0.f, 0.3f);

    setup_post_process_heightmap_attributes(
        node,
        {.add_mix = true, .remap_active_state = true});
  }

  // --- Group 3: Target

  {
    node.set_current_group(G_TARGET);

    node.set_current_category("Target Parameters");
    add_float(node, A_VALUE, "Value", 0.5f, -1.f, 2.f);
    add_float(node, A_WIDTH, "Width", 0.1f, 0.f, 0.3f);
    add_float(node, A_SMOOTHING, "Smoothing", 0.02f, 0.001f, 1.f);

    setup_post_process_heightmap_attributes(
        node,
        {.add_mix = false, .remap_active_state = true});
  }

  // --- Group 4: Midrange

  {
    node.set_current_group(G_MIDRANGE);

    node.set_current_category("Midrange Parameters");
    add_float(node, A_GAIN, "Gain", 1.f, 0.01f, 10.f);

    setup_post_process_heightmap_attributes(
        node,
        {.add_mix = true, .remap_active_state = true});
  }

  // Reset active group to first
  node.set_current_group(G_INTERVAL);
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_select_value_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_in  = node.get_value_ref<hmap::VirtualArray>(P_IN);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_in || !p_out)
    return;

  const std::string group = node.get_meta_group().current_container_name().value_or(
      G_INTERVAL);

  // --- Group Dispatch

  if (group == G_INTERVAL)
  {
    const auto value1 = node.val<float>(A_VALUE1);
    const auto value2 = node.val<float>(A_VALUE2);
    const auto width  = node.val<float>(A_WIDTH);

    const float xmin = std::min(value1, value2);
    const float xmax = std::max(value1, value2);

    hmap::for_each_tile(
        {p_in},
        {p_out},
        [&](std::vector<const hmap::Array *> in,
            std::vector<hmap::Array *>       out,
            const hmap::TileRegion &)
        {
          auto [pa_in]  = unpack<1>(in);
          auto [pa_out] = unpack<1>(out);

          *pa_out = hmap::sigmoid(*pa_in, width, 0.f, 1.f, xmin);
          *pa_out *= 1.f - hmap::sigmoid(*pa_in, width, 0.f, 1.f, xmax);
        },
        node.cfg().cm_cpu);

    p_out->smooth_overlap_buffers();
    post_process_heightmap(node, *p_out);
  }
  else if (group == G_THRESHOLD)
  {
    const auto width = node.val<float>(A_WIDTH);
    const auto x0    = node.val<float>(A_X0);

    hmap::for_each_tile(
        {p_in},
        {p_out},
        [&](std::vector<const hmap::Array *> in,
            std::vector<hmap::Array *>       out,
            const hmap::TileRegion &)
        {
          auto [pa_in]  = unpack<1>(in);
          auto [pa_out] = unpack<1>(out);

          *pa_out = hmap::sigmoid(*pa_in, width, 0.f, 1.f, x0);
        },
        node.cfg().cm_cpu);

    p_out->smooth_overlap_buffers();
    post_process_heightmap(node, *p_out, p_in);
  }
  else if (group == G_TARGET)
  {
    const auto value     = node.val<float>(A_VALUE);
    const auto width     = node.val<float>(A_WIDTH);
    const auto smoothing = node.val<float>(A_SMOOTHING);

    const float xmin = value - 0.5f * width;
    const float xmax = value + 0.5f * width;

    hmap::for_each_tile(
        {p_in},
        {p_out},
        [xmin, xmax, smoothing](std::vector<const hmap::Array *> in,
                                std::vector<hmap::Array *>       out,
                                const hmap::TileRegion &)
        {
          auto [pa_in]  = unpack<1>(in);
          auto [pa_out] = unpack<1>(out);

          *pa_out = hmap::sigmoid(*pa_in, smoothing, 0.f, 1.f, xmin);
          *pa_out *= 1.f - hmap::sigmoid(*pa_in, smoothing, 0.f, 1.f, xmax);
        },
        node.cfg().cm_cpu);

    p_out->smooth_overlap_buffers();
    post_process_heightmap(node, *p_out);
  }
  else if (group == G_MIDRANGE)
  {
    float vmin = p_in->min(node.cfg().cm_cpu);
    float vmax = p_in->max(node.cfg().cm_cpu);

    hmap::for_each_tile(
        {p_out, p_in},
        [&node, vmin, vmax](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);
          *pa_out = hmap::select_midrange(*pa_in, node.val<float>(A_GAIN), vmin, vmax);
        },
        node.cfg().cm_cpu);

    p_out->smooth_overlap_buffers();
    post_process_heightmap(node, *p_out);
  }
}

} // namespace hesiod
