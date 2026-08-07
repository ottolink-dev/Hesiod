/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/boundary.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/app/enum_mappings.hpp"
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

constexpr const char *A_OVERLAP          = "overlap";
constexpr const char *A_PERIODICITY_TYPE = "periodicity_type";

void setup_make_periodic_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  add_float(node, A_OVERLAP, "overlap", 0.25f, 0.05f, 0.5f);
  add_enum(node,
           A_PERIODICITY_TYPE,
           "periodicity_type",
           enum_mappings.periodicity_type_map,
           "X and Y");
}

void compute_make_periodic_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);

  if (p_in)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

    int nbuffer = std::max(1, (int)(node.val<float>(A_OVERLAP) * p_out->shape.x));

    hmap::for_each_tile(
        {p_out, p_in},
        [&node, nbuffer](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);
          *pa_out              = *pa_in;

          hmap::make_periodic(*pa_out,
                              nbuffer,
                              (hmap::PeriodicityType)node.val<int>(A_PERIODICITY_TYPE));
        },
        node.cfg().cm_single_array);
  }
}

} // namespace hesiod
