/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/geometry/path.hpp"
#include "highmap/shortest_path.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_HEIGHTMAP = "heightmap";
constexpr const char *P_MASK_NOGO = "mask nogo";
constexpr const char *P_PATH      = "path";
constexpr const char *P_WAYPOINTS = "waypoints";

constexpr const char *A_DISTANCE_EXPONENT = "distance_exponent";
constexpr const char *A_DOWNSAMPLING      = "downsampling";
constexpr const char *A_ELEVATION_RATIO   = "elevation_ratio";

void setup_path_find_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Path>(gnode::PortType::IN, P_WAYPOINTS);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_HEIGHTMAP);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK_NOGO);
  node.add_port<hmap::Path>(gnode::PortType::OUT, P_PATH);

  // attribute(s)
  add_float(node, A_ELEVATION_RATIO, "elevation_ratio", 0.1f, 0.f, 0.9f);
  add_float(node, A_DISTANCE_EXPONENT, "distance_exponent", 1.f, 0.5f, 2.f);
  add_int(node, A_DOWNSAMPLING, "downsampling", 4, 1, 10);
}

void compute_path_find_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Path         *p_waypoints = node.get_value_ref<hmap::Path>(P_WAYPOINTS);
  hmap::VirtualArray *p_hmap      = node.get_value_ref<hmap::VirtualArray>(P_HEIGHTMAP);

  if (p_waypoints && p_hmap)
  {
    hmap::VirtualArray *p_mask = node.get_value_ref<hmap::VirtualArray>(P_MASK_NOGO);
    hmap::Path         *p_out  = node.get_value_ref<hmap::Path>(P_PATH);

    // copy the input heightmap
    *p_out = *p_waypoints;

    if (p_out->size() > 1)
    {
      // working shape
      float      ds        = (float)node.val<int>(A_DOWNSAMPLING);
      glm::ivec2 shape_wrk = glm::ivec2((int)(p_hmap->shape.x / ds),
                                        (int)(p_hmap->shape.y / ds));

      shape_wrk.x = std::max(2, shape_wrk.x);
      shape_wrk.y = std::max(2, shape_wrk.y);

      Logger::log()->trace("working shape: ({}, {})", shape_wrk.x, shape_wrk.y);

      // work on a single array (as a temporary solution?)
      hmap::Array z  = p_hmap->to_array(node.cfg().cm_cpu);
      hmap::Array zw = z.resample_to_shape_nearest(shape_wrk);

      // handle masking
      hmap::Array *p_mask_array = nullptr;
      hmap::Array  mask_array;

      if (p_mask)
      {
        mask_array   = p_mask->to_array(node.cfg().cm_cpu);
        mask_array   = mask_array.resample_to_shape_nearest(shape_wrk);
        p_mask_array = &mask_array;
      }

      // perform Dijkstra's path finding
      glm::vec4 bbox(0.f, 1.f, 0.f, 1.f);
      int       edge_divisions = 0;

      *p_out = hmap::dijkstra(*p_out,
                              zw,
                              bbox,
                              edge_divisions,
                              node.val<float>(A_ELEVATION_RATIO),
                              node.val<float>(A_DISTANCE_EXPONENT),
                              p_mask_array);

      // set values based on the "fine" grid array
      p_out->set_values_from_array(z, bbox);
    }
  }
}

} // namespace hesiod
