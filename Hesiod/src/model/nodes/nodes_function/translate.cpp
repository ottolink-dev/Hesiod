/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/transform.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_DX  = "dx";
constexpr const char *P_DY  = "dy";
constexpr const char *P_IN  = "input";
constexpr const char *P_OUT = "output";

constexpr const char *A_CENTER   = "center";
constexpr const char *A_PERIODIC = "periodic";

void setup_translate_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DX);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DY);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  add_xy(node, A_CENTER, "center");
  add_bool(node, A_PERIODIC, "periodic", false);
}

void compute_translate_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // base noise function
  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);

  if (p_in)
  {
    hmap::VirtualArray *p_dx  = node.get_value_ref<hmap::VirtualArray>(P_DX);
    hmap::VirtualArray *p_dy  = node.get_value_ref<hmap::VirtualArray>(P_DY);
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

    hmap::for_each_tile(
        {p_out, p_in, p_dx, p_dy},
        [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out, pa_in, pa_dx, pa_dy] = unpack<4>(p_arrays);

          glm::vec2 center = node.val<glm::vec2>(A_CENTER);

          *pa_out = hmap::translate(*pa_in,
                                    center.x - 0.5f,
                                    center.y - 0.5f,
                                    node.val<bool>(A_PERIODIC),
                                    pa_dx,
                                    pa_dy);
        },
        node.cfg().cm_single_array);
  }
}

} // namespace hesiod
