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
constexpr const char *P_DR = "dr";
constexpr const char *P_DX = "dx";
constexpr const char *P_DY = "dy";

constexpr const char *A_CENTER    = "center";
constexpr const char *A_SMOOTHING = "smoothing";

void setup_radial_displacement_to_xy_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DR);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_DX, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_DY, CONFIG(node));

  // attribute(s)
  add_float(node, A_SMOOTHING, "smoothing", 1.f, 0.f, 10.f);
  add_xy(node, A_CENTER, "center");
}

void compute_radial_displacement_to_xy_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_dr = node.get_value_ref<hmap::VirtualArray>(P_DR);

  if (p_dr)
  {
    hmap::VirtualArray *p_dx = node.get_value_ref<hmap::VirtualArray>(P_DX);
    hmap::VirtualArray *p_dy = node.get_value_ref<hmap::VirtualArray>(P_DY);

    hmap::for_each_tile(
        {p_dr},
        {p_dx, p_dy},
        [&node](std::vector<const hmap::Array *> p_arrays_in,
                std::vector<hmap::Array *>       p_arrays_out,
                const hmap::TileRegion          &region)
        {
          auto [pa_dr]        = unpack<1>(p_arrays_in);
          auto [pa_dx, pa_dy] = unpack<2>(p_arrays_out);

          hmap::radial_displacement_to_xy(*pa_dr,
                                          *pa_dx,
                                          *pa_dy,
                                          node.val<float>(A_SMOOTHING),
                                          node.val<glm::vec2>(A_CENTER),
                                          region.bbox);
        },
        node.cfg().cm_cpu);
  }
}

} // namespace hesiod
