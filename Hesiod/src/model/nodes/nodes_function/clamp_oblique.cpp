/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/range.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_clamp_oblique_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("vmax", "Clamp Value", 0.5f, -FLT_MAX, FLT_MAX);
  node.add_attr<FloatAttribute>("angle", "Angle", 0.f, -180.f, 180.f, "{:.1f}°");
  node.add_attr<FloatAttribute>("slope", "Slope", 0.2f, 0.f, 4.f);
  node.add_attr<Vec2FloatAttribute>("center", "Center");
  node.add_attr<BoolAttribute>("use_max_operator", "Clamp Mode", "Max", "Min", true);
  node.add_attr<FloatAttribute>("k", "Smoothing", 0.f, 0.f, 1.f);

  // attribute(s) order
  node.set_attr_ordered_key({"_GROUPBOX_BEGIN_Clamp Parameters",
                             "vmax",
                             "angle",
                             "slope",
                             "center",
                             "use_max_operator",
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Smoothing",
                             "k",
                             "_GROUPBOX_END_"});

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

void compute_clamp_oblique_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // base noise function
  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>("input");

  if (p_in)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");

    hmap::for_each_tile(
        {p_out, p_in},
        [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &region)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);
          *pa_out = *pa_in;

          hmap::clamp_oblique_plane(*pa_out,
                                    node.get_attr<FloatAttribute>("vmax"),
                                    node.get_attr<FloatAttribute>("angle"),
                                    node.get_attr<FloatAttribute>("slope"),
                                    node.get_attr<BoolAttribute>("use_max_operator"),
                                    node.get_attr<FloatAttribute>("k"),
                                    node.get_attr<Vec2FloatAttribute>("center"),
                                    region.bbox);
        },
        node.cfg().cm_cpu);

    // post-process
    post_process_heightmap(node, *p_out, p_in);
  }
}

} // namespace hesiod
