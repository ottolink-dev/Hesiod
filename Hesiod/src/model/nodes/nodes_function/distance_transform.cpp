/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/filters.hpp"
#include "highmap/morphology.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_distance_transform_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<EnumAttribute>("transform_type",
                               "transform_type",
                               enum_mappings.distance_transform_type_map,
                               "Approx. (fast)");
  node.add_attr<BoolAttribute>("reverse_input", "reverse_input", false);
  node.add_attr<FloatAttribute>("threshold", "threshold", 0.f, -1.f, 2.f);

  // attribute(s) order
  node.set_attr_ordered_key({"_GROUPBOX_BEGIN_Main Parameters",
                             "transform_type",
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Input preprocessing",
                             "reverse_input",
                             "threshold",
                             "_GROUPBOX_END_"});

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = true});
}

void compute_distance_transform_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>("input");

  if (p_in)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");

    hmap::for_each_tile(
        {p_out, p_in},
        [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);
          *pa_out              = *pa_in;
          make_binary(*pa_out, node.get_attr<FloatAttribute>("threshold"));

          if (node.get_attr<BoolAttribute>("reverse_input"))
            *pa_out = 1.f - *pa_out;

          auto type = static_cast<hmap::DistanceTransformType>(
              node.get_attr<EnumAttribute>("transform_type"));

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
