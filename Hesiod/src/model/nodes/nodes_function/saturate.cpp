/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/filters.hpp"
#include "highmap/operator.hpp"

#include "meta/core/data_provider.hpp"
#include "meta/metadata/keys.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_IN  = "input";
constexpr const char *P_OUT = "output";

constexpr const char *A_K_SMOOTHING = "k_smoothing";
constexpr const char *A_RANGE       = "range";

void setup_saturate_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  node.set_current_category("Main Parameters");
  add_float(node, A_K_SMOOTHING, "k_smoothing", 0.1f, 0.01f, 1.f);
  add_range(node, A_RANGE, "Saturation Range");

  setup_histogram_for_range_attribute(node, A_RANGE, P_IN);

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

void compute_saturate_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  auto *p_in  = node.get_value_ref<hmap::VirtualArray>(P_IN);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_in)
    return;

  float hmin = p_in->min(node.cfg().cm_cpu);
  float hmax = p_in->max(node.cfg().cm_cpu);

  auto &c = node.get_meta_group().current();

  const glm::vec2 range = c.value<glm::vec2>(A_RANGE);
  const float     k     = c.value<float>(A_K_SMOOTHING);

  hmap::for_each_tile(
      {p_out, p_in},
      [&node, &hmin, &hmax, &range, &k](std::vector<hmap::Array *> p_arrays,
                                        const hmap::TileRegion &)
      {
        auto [pa_out, pa_in] = unpack<2>(p_arrays);
        *pa_out              = *pa_in;

        hmap::saturate(*pa_out, range.x, range.y, hmin, hmax, k);
      },
      node.cfg().cm_cpu);

  // post-process
  post_process_heightmap(node, *p_out, p_in);
}

} // namespace hesiod
