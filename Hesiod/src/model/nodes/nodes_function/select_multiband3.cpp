/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
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
constexpr const char *P_HIGH = "high";
constexpr const char *P_IN   = "input";
constexpr const char *P_LOW  = "low";
constexpr const char *P_MID  = "mid";

constexpr const char *A_OVERLAP = "overlap";
constexpr const char *A_RATIO1  = "ratio1";
constexpr const char *A_RATIO2  = "ratio2";

void setup_select_multiband3_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_LOW, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_MID, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_HIGH, CONFIG(node));

  // attribute(s)
  add_float(node, A_RATIO1, "ratio1", 0.2f, 0.f, 1.f);
  add_float(node, A_RATIO2, "ratio2", 0.5f, 0.f, 1.f);
  add_float(node, A_OVERLAP, "overlap", 0.5f, 0.f, 1.f);

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = false, .remap_active_state = true});
}

void compute_select_multiband3_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);

  if (p_in)
  {
    hmap::VirtualArray *p_low  = node.get_value_ref<hmap::VirtualArray>(P_LOW);
    hmap::VirtualArray *p_mid  = node.get_value_ref<hmap::VirtualArray>(P_MID);
    hmap::VirtualArray *p_high = node.get_value_ref<hmap::VirtualArray>(P_HIGH);

    float vmin = p_in->min(node.cfg().cm_cpu);
    float vmax = p_in->max(node.cfg().cm_cpu);

    hmap::for_each_tile(
        {p_in, p_low, p_mid, p_high},
        [&node, vmin, vmax](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_in, pa_low, pa_mid, pa_high] = unpack<4>(p_arrays);

          hmap::select_multiband3(*pa_in,
                                  *pa_low,
                                  *pa_mid,
                                  *pa_high,
                                  node.val<float>(A_RATIO1),
                                  node.val<float>(A_RATIO2),
                                  node.val<float>(A_OVERLAP),
                                  vmin,
                                  vmax);
        },
        node.cfg().cm_cpu);

    // post-process
    post_process_heightmap(node, *p_low);
    post_process_heightmap(node, *p_mid);
    post_process_heightmap(node, *p_high);
  }
}

} // namespace hesiod
