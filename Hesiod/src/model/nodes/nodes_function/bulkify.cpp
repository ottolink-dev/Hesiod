/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/filters.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_bulkify_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "dx");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "dy");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("amplitude", "amplitude", 1.f, -1.f, 4.f);
  node.add_attr<EnumAttribute>("bulk_type",
                               "bulk_type",
                               enum_mappings.primitive_type_map);
  node.add_attr<Vec2FloatAttribute>("center", "center");

  // attribute(s) order
  node.set_attr_ordered_key({"_GROUPBOX_BEGIN_Main Parameters",
                             "_TEXT_Bulk Shape",
                             "amplitude",
                             "bulk_type",
                             "_TEXT_Position",
                             "center",
                             "_GROUPBOX_END_"});

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

void compute_bulkify_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>("input");

  if (p_in)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");
    hmap::VirtualArray *p_dx = node.get_value_ref<hmap::VirtualArray>("dx");
    hmap::VirtualArray *p_dy = node.get_value_ref<hmap::VirtualArray>("dy");

    hmap::for_each_tile(
        {p_out, p_in, p_dx, p_dy},
        [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &region)
        {
          auto [pa_out, pa_in, pa_dx, pa_dy] = unpack<4>(p_arrays);

          *pa_out = hmap::bulkify(
              *pa_in,
              (hmap::PrimitiveType)node.get_attr<EnumAttribute>("bulk_type"),
              node.get_attr<FloatAttribute>("amplitude"),
              pa_dx,
              pa_dy,
              node.get_attr<Vec2FloatAttribute>("center"),
              region.bbox);
        },
        node.cfg().cm_cpu);

    p_out->remap(p_in->min(node.cfg().cm_cpu),
                 p_in->max(node.cfg().cm_cpu),
                 node.cfg().cm_cpu);
    post_process_heightmap(node, *p_out, p_in);
  }
}

} // namespace hesiod
