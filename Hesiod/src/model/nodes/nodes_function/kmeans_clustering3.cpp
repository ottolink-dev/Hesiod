/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/features.hpp"
#include "highmap/range.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_kmeans_clustering3_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "feature 1");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "feature 2");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "feature 3");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<SeedAttribute>("seed", "Seed");
  node.add_attr<IntAttribute>("nclusters", "nclusters", 6, 1, 16);
  node.add_attr<FloatAttribute>("weights.x", "weights.x", 1.f, 0.01f, 2.f);
  node.add_attr<FloatAttribute>("weights.y", "weights.y", 1.f, 0.01f, 2.f);
  node.add_attr<FloatAttribute>("weights.z", "weights.z", 1.f, 0.01f, 2.f);
  node.add_attr<BoolAttribute>("normalize_inputs", "normalize_inputs", true);

  // attribute(s) order
  node.set_attr_ordered_key(
      {"seed", "nclusters", "weights.x", "weights.y", "weights.z", "normalize_inputs"});
}

void compute_kmeans_clustering3_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // base noise function
  hmap::VirtualArray *p_in1 = node.get_value_ref<hmap::VirtualArray>("feature 1");
  hmap::VirtualArray *p_in2 = node.get_value_ref<hmap::VirtualArray>("feature 2");
  hmap::VirtualArray *p_in3 = node.get_value_ref<hmap::VirtualArray>("feature 3");
  hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");

  if (p_in1 && p_in2 && p_in3)
  {
    hmap::for_each_tile(
        {p_out, p_in1, p_in2, p_in3},
        [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out, pa_in1, pa_in2, pa_in3] = unpack<4>(p_arrays);

          if (node.get_attr<BoolAttribute>("normalize_inputs"))
          {
            hmap::remap(*pa_in1);
            hmap::remap(*pa_in2);
            hmap::remap(*pa_in3);
          }

          glm::vec3 weights = {node.get_attr<FloatAttribute>("weights.x"),
                               node.get_attr<FloatAttribute>("weights.y"),
                               node.get_attr<FloatAttribute>("weights.z")};

          *pa_out = hmap::kmeans_clustering3(*pa_in1,
                                             *pa_in2,
                                             *pa_in3,
                                             node.get_attr<IntAttribute>("nclusters"),
                                             nullptr, // scoring_arrays,
                                             nullptr, // agg scoring
                                             weights,
                                             node.get_attr<SeedAttribute>("seed"));
        },
        node.cfg().cm_single_array);
  }
}

} // namespace hesiod
