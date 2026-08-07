/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/filters.hpp"
#include "highmap/morphology.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_IN  = "input";
constexpr const char *P_OUT = "output";

constexpr const char *A_REVERSE_INPUT  = "reverse_input";
constexpr const char *A_THRESHOLD      = "threshold";
constexpr const char *A_TRANSFORM_TYPE = "transform_type";

void setup_distance_transform_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  add_enum(node,
           A_TRANSFORM_TYPE,
           "transform_type",
           enum_mappings.distance_transform_type_map,
           "Approx. (fast)");
  add_bool(node, A_REVERSE_INPUT, "reverse_input", false);
  add_float(node, A_THRESHOLD, "threshold", 0.f, -1.f, 2.f);

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = true});
}

void compute_distance_transform_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);

  if (p_in)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

    hmap::for_each_tile(
        {p_out, p_in},
        [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);
          *pa_out              = *pa_in;
          make_binary(*pa_out, node.val<float>(A_THRESHOLD));

          if (node.val<bool>(A_REVERSE_INPUT))
            *pa_out = 1.f - *pa_out;

          auto type = static_cast<hmap::DistanceTransformType>(
              node.val<int>(A_TRANSFORM_TYPE));

          switch (type)
          {
          case hmap::DistanceTransformType::DT_EXACT:
            *pa_out = hmap::distance_transform(*pa_out);
            break;
          case hmap::DistanceTransformType::DT_JFA:
            *pa_out = hmap::gpu::distance_transform_jfa(*pa_out);
            break;
          case hmap::DistanceTransformType::DT_MANHATTAN:
            *pa_out = hmap::distance_transform_manhattan(*pa_out);
            break;
          case hmap::DistanceTransformType::DT_APPROX:
          default:
            *pa_out = hmap::distance_transform_approx(*pa_out);
            break;
          }
        },
        node.cfg().cm_single_array); // mandatory

    // post-process
    post_process_heightmap(node, *p_out);
  }
}

} // namespace hesiod
