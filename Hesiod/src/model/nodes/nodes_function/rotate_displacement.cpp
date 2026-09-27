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
constexpr const char *P_DELTA = "delta";
constexpr const char *P_DX    = "dx";
constexpr const char *P_DY    = "dy";

constexpr const char *A_ANGLE = "angle";

void setup_rotate_displacement_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DELTA);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_DX, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_DY, CONFIG(node));

  // attribute(s)
  add_float(node, A_ANGLE, "angle", 0.f, -180.f, 180.f);
}

void compute_rotate_displacement_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(P_DELTA);

  if (p_in)
  {
    hmap::VirtualArray *p_dx = node.get_value_ref<hmap::VirtualArray>(P_DX);
    hmap::VirtualArray *p_dy = node.get_value_ref<hmap::VirtualArray>(P_DY);

    hmap::for_each_tile(
        {p_in},
        {p_dx, p_dy},
        [&node](std::vector<const hmap::Array *> p_arrays_in,
                std::vector<hmap::Array *>       p_arrays_out,
                const hmap::TileRegion &)
        {
          auto [pa_in]        = unpack<1>(p_arrays_in);
          auto [pa_dx, pa_dy] = unpack<2>(p_arrays_out);

          hmap::rotate_displacement(*pa_in, node.val<float>(A_ANGLE), *pa_dx, *pa_dy);
        },
        node.cfg().cm_cpu);
  }
}

} // namespace hesiod
