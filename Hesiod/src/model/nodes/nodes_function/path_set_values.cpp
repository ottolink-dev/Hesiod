/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/geometry/path.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_PATH      = "path";
constexpr const char *P_HEIGHTMAP = "heightmap";
constexpr const char *P_OUT       = "output";

// Group: From Heightmap
constexpr const char *G_FROM_HEIGHTMAP = "From Heightmap";

// Group: From Border Distance
constexpr const char *G_FROM_BORDER_DISTANCE = "From Border Distance";

// Group: From Min Distance
constexpr const char *G_FROM_MIN_DISTANCE = "From Min Distance";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_path_set_values_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Path>(gnode::PortType::IN, P_PATH);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_HEIGHTMAP);
  node.add_port<hmap::Path>(gnode::PortType::OUT, P_OUT);

  // Group: From Heightmap
  {
    node.set_current_group(G_FROM_HEIGHTMAP);
    add_comment(node, "comment", "", "No Parameter.");
  }

  // Group: From Border Distance
  {
    node.set_current_group(G_FROM_BORDER_DISTANCE);
    add_comment(node, "comment", "", "No Parameter.");
  }

  // Group: From Min Distance
  {
    node.set_current_group(G_FROM_MIN_DISTANCE);
    add_comment(node, "comment", "", "No Parameter.");
  }
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_path_set_values_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Path         *p_path = node.get_value_ref<hmap::Path>(P_PATH);
  hmap::Path         *p_out  = node.get_value_ref<hmap::Path>(P_OUT);
  hmap::VirtualArray *p_hmap = node.get_value_ref<hmap::VirtualArray>(P_HEIGHTMAP);

  if (!p_path)
    return;

  const std::string current_group = node.get_meta_group()
                                        .current_container_name()
                                        .value_or(G_FROM_HEIGHTMAP);

  Logger::log()->trace("compute_path_set_values_node: current_group {}", current_group);

  if (current_group == G_FROM_HEIGHTMAP)
  {
    if (p_hmap)
    {
      *p_out = *p_path;

      hmap::for_each_tile(
          {p_hmap},
          {},
          [&](std::vector<const hmap::Array *> p_arrays_in,
              std::vector<hmap::Array *>,
              const hmap::TileRegion &region)
          {
            auto [pa_hmap] = unpack<1>(p_arrays_in);
            p_out->set_values_from_array(*pa_hmap, region.bbox);
          },
          node.cfg().cm_cpu);
    }
  }
  else if (current_group == G_FROM_BORDER_DISTANCE)
  {
    *p_out = *p_path;
    p_out->set_values_from_border_distance();
  }
  else if (current_group == G_FROM_MIN_DISTANCE)
  {
    *p_out = *p_path;
    p_out->set_values_from_min_distance();
  }
  else
  {
    Logger::log()->error("compute_path_set_values_node: group {} not implemented",
                         current_group);
  }
}

} // namespace hesiod
