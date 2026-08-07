/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/math.hpp"
#include "highmap/range.hpp"

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

constexpr const char *A_ORDER = "order";

void setup_smoothstep_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  std::vector<std::string> choices = {"3rd", "5th", "7th"};
  add_choice(node, A_ORDER, "order", choices);
}

void compute_smoothstep_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);

  if (p_in)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

    float hmin = p_in->min(node.cfg().cm_cpu);
    float hmax = p_in->max(node.cfg().cm_cpu);

    hmap::for_each_tile(
        {p_out, p_in},
        [&node, hmin, hmax](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);
          *pa_out              = *pa_in;

          hmap::remap(*pa_out, 0.f, 1.f, hmin, hmax);

          if (node.val<std::string>(A_ORDER) == "3rd")
            *pa_out = hmap::smoothstep3(*pa_out);
          else if (node.val<std::string>(A_ORDER) == "5th")
            *pa_out = hmap::smoothstep5(*pa_out);
          else if (node.val<std::string>(A_ORDER) == "7th")
            *pa_out = hmap::smoothstep7(*pa_out);

          hmap::remap(*pa_out, hmin, hmax, 0.f, 1.f);
        },
        node.cfg().cm_cpu);
  }
}

} // namespace hesiod
