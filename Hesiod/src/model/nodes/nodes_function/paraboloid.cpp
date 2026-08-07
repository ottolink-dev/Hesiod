/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/primitives.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_paraboloid_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "dx");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "dy");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "envelope");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("angle", "angle", 0.f, -180.f, 180.f);
  node.add_attr<FloatAttribute>("a", "a", 1.f, 0.01f, 5.f);
  node.add_attr<FloatAttribute>("b", "b", 1.f, 0.01f, 5.f);
  node.add_attr<FloatAttribute>("v0", "v0", 0.f, -2.f, 2.f);
  node.add_attr<BoolAttribute>("reverse_x", "reverse_x", false);
  node.add_attr<BoolAttribute>("reverse_y", "reverse_y", false);
  node.add_attr<Vec2FloatAttribute>("center", "center");
  // attribute(s) order
  node.set_attr_ordered_key(
      {"angle", "a", "b", "v0", "reverse_x", "reverse_y", "center"});

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = false, .remap_active_state = true});
}

void compute_paraboloid_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // base noise function
  hmap::VirtualArray *p_dx  = node.get_value_ref<hmap::VirtualArray>("dx");
  hmap::VirtualArray *p_dy  = node.get_value_ref<hmap::VirtualArray>("dy");
  hmap::VirtualArray *p_env = node.get_value_ref<hmap::VirtualArray>("envelope");
  hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");

  hmap::for_each_tile(
      {p_out, p_dx, p_dy},
      [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &region)
      {
        auto [pa_out, pa_dx, pa_dy] = unpack<3>(p_arrays);

        *pa_out = hmap::paraboloid(region.shape,
                                   node.get_attr<FloatAttribute>("angle"),
                                   node.get_attr<FloatAttribute>("a"),
                                   node.get_attr<FloatAttribute>("b"),
                                   node.get_attr<FloatAttribute>("v0"),
                                   node.get_attr<BoolAttribute>("reverse_x"),
                                   node.get_attr<BoolAttribute>("reverse_y"),
                                   pa_dx,
                                   pa_dy,
                                   nullptr,
                                   node.get_attr<Vec2FloatAttribute>("center"),
                                   region.bbox);
      },
      node.cfg().cm_cpu);

  // post-process
  post_apply_enveloppe(node, *p_out, p_env);
  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
