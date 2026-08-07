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

constexpr const char *A_CENTER      = "center";
constexpr const char *A_PERIODIC    = "periodic";
constexpr const char *A_REMAP       = "remap";
constexpr const char *A_ZOOM_FACTOR = "zoom_factor";

void setup_zoom_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DX);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DY);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  add_float(node, A_ZOOM_FACTOR, "zoom_factor", 2.f, 1.f, 10.f);
  add_bool(node, A_PERIODIC, "periodic", false);
  add_xy(node, A_CENTER, "center");
  add_bool(node, A_REMAP, "remap", false);
}

void compute_zoom_node(BaseNode &node)
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

          *pa_out = hmap::zoom(*pa_in,
                               node.val<float>(A_ZOOM_FACTOR),
                               node.val<bool>(A_PERIODIC),
                               node.val<glm::vec2>(A_CENTER),
                               pa_dx,
                               pa_dy);
        },
        node.cfg().cm_single_array);

    if (node.val<bool>(A_REMAP))
      p_out->remap(0.f, 1.f, node.cfg().cm_cpu);
  }
}

} // namespace hesiod
