/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/filters.hpp"
#include "highmap/operator.hpp"
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
constexpr const char *P_IN   = "input";
constexpr const char *P_MASK = "mask";
constexpr const char *P_OUT  = "output";

constexpr const char *A_VALUES = "values";

void setup_recurve_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  std::vector<float> default_values = {0.f, 0.25f, 0.5f, 0.75f, 1.f};
  add_curve(node, A_VALUES, "values", default_values, 0.f, 1.f);
}

void compute_recurve_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);

  if (p_in)
  {
    hmap::VirtualArray *p_mask = node.get_value_ref<hmap::VirtualArray>(P_MASK);
    hmap::VirtualArray *p_out  = node.get_value_ref<hmap::VirtualArray>(P_OUT);

    if (node.val<std::vector<float>>(A_VALUES).size() >= 3)
    {
      float hmin = p_in->min(node.cfg().cm_cpu);
      float hmax = p_in->max(node.cfg().cm_cpu);

      std::vector<float> t = hmap::linspace(
          0.f,
          1.f,
          node.val<std::vector<float>>(A_VALUES).size());

      hmap::for_each_tile(
          {p_out, p_in, p_mask},
          [&node, t, hmin, hmax](std::vector<hmap::Array *> p_arrays,
                                 const hmap::TileRegion &)
          {
            auto [pa_out, pa_in, pa_mask] = unpack<3>(p_arrays);

            *pa_out = *pa_in;

            hmap::remap(*pa_out, 0.f, 1.f, hmin, hmax);
            hmap::recurve(*pa_out, t, node.val<std::vector<float>>(A_VALUES), pa_mask);
            hmap::remap(*pa_out, hmin, hmax, 0.f, 1.f);
          },
          node.cfg().cm_cpu);
    }
    else
    {
      // don't do anything if not enough values
      Logger::log()->warn("not enough values, at least 4 points required");
    }
  }
}

} // namespace hesiod
