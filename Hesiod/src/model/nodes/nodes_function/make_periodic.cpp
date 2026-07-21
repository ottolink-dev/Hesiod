/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/boundary.hpp"

#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_make_periodic_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("overlap", "overlap", 0.25f, 0.05f, 0.5f);
  node.add_attr<EnumAttribute>("periodicity_type",
                               "periodicity_type",
                               enum_mappings.periodicity_type_map,
                               "X and Y");

  // attribute(s) order
  node.set_attr_ordered_key({"overlap", "periodicity_type"});
}

void compute_make_periodic_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>("input");

  if (p_in)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");

    int nbuffer = std::max(
        1,
        (int)(node.get_attr<FloatAttribute>("overlap") * p_out->shape.x));

    hmap::for_each_tile(
        {p_out, p_in},
        [&node, nbuffer](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);
          *pa_out = *pa_in;

          hmap::make_periodic(
              *pa_out,
              nbuffer,
              (hmap::PeriodicityType)node.get_attr<EnumAttribute>("periodicity_type"));
        },
        node.cfg().cm_single_array);
  }
}

} // namespace hesiod
