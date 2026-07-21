/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/geometry/path.hpp"
#include "highmap/shortest_path.hpp"

#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_path_find_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Path>(gnode::PortType::IN, "waypoints");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "heightmap");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "mask nogo");
  node.add_port<hmap::Path>(gnode::PortType::OUT, "path");

  // attribute(s)
  node.add_attr<FloatAttribute>("elevation_ratio", "elevation_ratio", 0.1f, 0.f, 0.9f);
  node.add_attr<FloatAttribute>("distance_exponent", "distance_exponent", 1.f, 0.5f, 2.f);
  node.add_attr<IntAttribute>("downsampling", "downsampling", 4, 1, 10);

  // attribute(s) order
  node.set_attr_ordered_key({"elevation_ratio", "distance_exponent", "downsampling"});
}

void compute_path_find_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Path         *p_waypoints = node.get_value_ref<hmap::Path>("waypoints");
  hmap::VirtualArray *p_hmap = node.get_value_ref<hmap::VirtualArray>("heightmap");

  if (p_waypoints && p_hmap)
  {
    hmap::VirtualArray *p_mask = node.get_value_ref<hmap::VirtualArray>("mask nogo");
    hmap::Path         *p_out = node.get_value_ref<hmap::Path>("path");

    // copy the input heightmap
    *p_out = *p_waypoints;

    if (p_out->size() > 1)
    {
      // working shape
      float      ds = (float)node.get_attr<IntAttribute>("downsampling");
      glm::ivec2 shape_wrk = glm::ivec2((int)(p_hmap->shape.x / ds),
                                        (int)(p_hmap->shape.y / ds));

      shape_wrk.x = std::max(2, shape_wrk.x);
      shape_wrk.y = std::max(2, shape_wrk.y);

      Logger::log()->trace("working shape: ({}, {})", shape_wrk.x, shape_wrk.y);

      // work on a single array (as a temporary solution?)
      hmap::Array z = p_hmap->to_array(node.cfg().cm_cpu);
      hmap::Array zw = z.resample_to_shape_nearest(shape_wrk);

      // handle masking
      hmap::Array *p_mask_array = nullptr;
      hmap::Array  mask_array;

      if (p_mask)
      {
        mask_array = p_mask->to_array(node.cfg().cm_cpu);
        mask_array = mask_array.resample_to_shape_nearest(shape_wrk);
        p_mask_array = &mask_array;
      }

      // perform Dijkstra's path finding
      glm::vec4 bbox(0.f, 1.f, 0.f, 1.f);
      int       edge_divisions = 0;

      *p_out = hmap::dijkstra(*p_out,
                              zw,
                              bbox,
                              edge_divisions,
                              node.get_attr<FloatAttribute>("elevation_ratio"),
                              node.get_attr<FloatAttribute>("distance_exponent"),
                              p_mask_array);

      // set values based on the "fine" grid array
      p_out->set_values_from_array(z, bbox);
    }
  }
}

} // namespace hesiod
