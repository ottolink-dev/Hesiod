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
constexpr const char *P_IN  = "input";
constexpr const char *P_OUT = "output";

constexpr const char *A_CLIPPING_RATIO = "clipping_ratio";
constexpr const char *A_TALUS_REF      = "talus_ref";

void setup_select_rivers_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  add_float(node, A_TALUS_REF, "talus_ref", 0.1f, 0.01f, 10.f);
  add_float(node, A_CLIPPING_RATIO, "clipping_ratio", 50.f, 0.1f, 100.f);

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = false, .remap_active_state = true});
}

void compute_select_rivers_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);

  if (p_in)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

    hmap::for_each_tile(
        {p_out, p_in},
        [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);

          *pa_out = hmap::select_rivers(*pa_in,
                                        node.val<float>(A_TALUS_REF),
                                        node.val<float>(A_CLIPPING_RATIO));
        },
        node.cfg().cm_single_array);

    // post-process
    post_process_heightmap(node, *p_out);
  }
}

} // namespace hesiod
