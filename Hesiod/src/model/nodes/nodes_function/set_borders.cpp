/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/boundary.hpp"

#include "attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_set_borders_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("radius", "radius", 0.4f, 0.f, 0.5f);
  node.add_attr<BoolAttribute>("unique_border_radius", "unique_border_radius", true);
  node.add_attr<FloatAttribute>("radius_west", "radius_west", 0.4f, 0.f, 0.5f);
  node.add_attr<FloatAttribute>("radius_east", "radius_east", 0.4f, 0.f, 0.5f);
  node.add_attr<FloatAttribute>("radius_north", "radius_north", 0.4f, 0.f, 0.5f);
  node.add_attr<FloatAttribute>("radius_south", "radius_south", 0.4f, 0.f, 0.5f);
  node.add_attr<BoolAttribute>("unique_border_value", "unique_border_value", false);
  node.add_attr<FloatAttribute>("value_west", "value_west", -0.5f, -FLT_MAX, FLT_MAX);
  node.add_attr<FloatAttribute>("value_east", "value_east", 0.5f, -FLT_MAX, FLT_MAX);
  node.add_attr<FloatAttribute>("value_north", "value_north", 0.5f, -FLT_MAX, FLT_MAX);
  node.add_attr<FloatAttribute>("value_south", "value_south", -0.5f, -FLT_MAX, FLT_MAX);

  // attribute(s) order
  node.set_attr_ordered_key({"radius",
                             "unique_border_radius",
                             "radius_west",
                             "radius_east",
                             "radius_north",
                             "radius_south",
                             "_SEPARATOR_",
                             "unique_border_value",
                             "value_west",
                             "value_east",
                             "value_north",
                             "value_south"});
}

void compute_set_borders_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>("input");

  if (p_in)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");

    // Per-edge buffer thickness, scaled by the axis each edge runs along
    // (E/W along x, N/S along y) so caps are predictable on non-square maps.
    auto to_px_x = [&](float r) { return std::max(1, (int)(r * p_in->shape.x)); };
    auto to_px_y = [&](float r) { return std::max(1, (int)(r * p_in->shape.y)); };

    int rw, re, rn, rs;
    if (node.get_attr<BoolAttribute>("unique_border_radius"))
    {
      const float r = node.get_attr<FloatAttribute>("radius");
      rw = re = to_px_x(r);
      rn = rs = to_px_y(r);
    }
    else
    {
      rw = to_px_x(node.get_attr<FloatAttribute>("radius_west"));
      re = to_px_x(node.get_attr<FloatAttribute>("radius_east"));
      rn = to_px_y(node.get_attr<FloatAttribute>("radius_north"));
      rs = to_px_y(node.get_attr<FloatAttribute>("radius_south"));
    }

    // engine order: {west=.x, east=.y, south=.z, north=.w}
    glm::ivec4 buffer_sizes(rw, re, rs, rn);

    const float vw = node.get_attr<FloatAttribute>("value_west");
    const float ve = node.get_attr<FloatAttribute>("value_east");
    const float vn = node.get_attr<FloatAttribute>("value_north");
    const float vs = node.get_attr<FloatAttribute>("value_south");

    glm::vec4 border_values;
    if (node.get_attr<BoolAttribute>("unique_border_value"))
      border_values = {vw, vw, vw, vw};
    else
      border_values = {vw, ve, vs, vn}; // {west, east, south, north} per engine

    hmap::for_each_tile(
        {p_out, p_in},
        [border_values, buffer_sizes](std::vector<hmap::Array *> p_arrays,
                                      const hmap::TileRegion &)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);
          *pa_out = *pa_in;

          hmap::set_borders(*pa_out, border_values, buffer_sizes);
        },
        node.cfg().cm_single_array);
  }
}

} // namespace hesiod
