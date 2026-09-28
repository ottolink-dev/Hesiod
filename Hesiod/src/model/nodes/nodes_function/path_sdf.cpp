/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
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
constexpr const char *P_DX   = "dx";
constexpr const char *P_DY   = "dy";
constexpr const char *P_PATH = "path";
constexpr const char *P_SDF  = "sdf";

void setup_path_sdf_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Path>(gnode::PortType::IN, P_PATH);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DX);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DY);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_SDF, CONFIG(node));

  // attribute(s)

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = false, .remap_active_state = true});
}

void compute_path_sdf_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Path *p_path = node.get_value_ref<hmap::Path>(P_PATH);

  if (p_path)
  {
    hmap::VirtualArray *p_dx  = node.get_value_ref<hmap::VirtualArray>(P_DX);
    hmap::VirtualArray *p_dy  = node.get_value_ref<hmap::VirtualArray>(P_DY);
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>(P_SDF);

    if (p_path->size() > 1)
    {
      hmap::for_each_tile(
          {p_dx, p_dy},
          {p_out},
          [&node, p_path](std::vector<const hmap::Array *> p_arrays_in,
                          std::vector<hmap::Array *>       p_arrays_out,
                          const hmap::TileRegion          &region)
          {
            auto [pa_dx, pa_dy] = unpack<2>(p_arrays_in);
            auto [pa_out]       = unpack<1>(p_arrays_out);

            *pa_out = hmap::path_sdf_to_array(*p_path,
                                              region.shape,
                                              region.bbox,
                                              pa_dx,
                                              pa_dy);
          },
          node.cfg().cm_cpu);

      // post-process
      post_process_heightmap(node, *p_out);
    }
    else
    {
      // fill with zeros
      hmap::for_each_tile(
          {},
          {p_out},
          [](std::vector<const hmap::Array *> p_arrays_in,
             std::vector<hmap::Array *>       p_arrays_out,
             const hmap::TileRegion &)
          {
            auto [pa_out] = unpack<1>(p_arrays_out);
            *pa_out       = 0.f;
          },
          node.cfg().cm_cpu);
    }
  }
}

} // namespace hesiod
