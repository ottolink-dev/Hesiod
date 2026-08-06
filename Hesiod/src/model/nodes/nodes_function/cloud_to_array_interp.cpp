/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_CLOUD = "cloud";
constexpr const char *P_DX = "dx";
constexpr const char *P_DY = "dy";
constexpr const char *P_OUT = "heightmap";

constexpr const char *A_ITP_METHOD = "itp_method";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_cloud_to_array_interp_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Cloud>(gnode::PortType::IN, P_CLOUD);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DX);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DY);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  node.add_attr<EnumAttribute>(A_ITP_METHOD,
                               "Interpolation Method",
                               enum_mappings.interpolation_method2d_map);

  // attribute(s) order
  node.set_attr_ordered_key({A_ITP_METHOD});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_cloud_to_array_interp_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Cloud *p_cloud = node.get_value_ref<hmap::Cloud>(P_CLOUD);

  if (!p_cloud)
    return;

  hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (p_cloud->size() > 0)
  {
    hmap::VirtualArray *p_dx = node.get_value_ref<hmap::VirtualArray>(P_DX);
    hmap::VirtualArray *p_dy = node.get_value_ref<hmap::VirtualArray>(P_DY);

    hmap::for_each_tile(
        {p_out, p_dx, p_dy},
        [&node, p_cloud](std::vector<hmap::Array *> p_arrays,
                         const hmap::TileRegion    &region)
        {
          hmap::Array *pa_out = p_arrays[0];
          hmap::Array *pa_dx = p_arrays[1];
          hmap::Array *pa_dy = p_arrays[2];

          glm::vec4 bbox_points = {0.f, 1.f, 0.f, 1.f};

          *pa_out = 0.f;

          hmap::InterpolationMethod2D method = hmap::InterpolationMethod2D(
              node.get_attr<EnumAttribute>(A_ITP_METHOD));

          p_cloud
              ->to_array_interp(*pa_out, bbox_points, method, pa_dx, pa_dy, region.bbox);
        },
        node.cfg().cm_cpu);
  }
  else
  {
    // fill with zeroes
    hmap::for_each_tile(
        {p_out},
        [](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          hmap::Array *pa_out = p_arrays[0];
          *pa_out = 0.f;
        },
        node.cfg().cm_cpu);
  }
}

} // namespace hesiod
