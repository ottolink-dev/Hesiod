/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/features.hpp"
#include "highmap/range.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_FEATURE_1 = "feature 1";
constexpr const char *P_FEATURE_2 = "feature 2";
constexpr const char *P_OUT       = "output";

constexpr const char *A_NCLUSTERS        = "nclusters";
constexpr const char *A_NORMALIZE_INPUTS = "normalize_inputs";
constexpr const char *A_SEED             = "seed";
constexpr const char *A_WEIGHTS_X        = "weights.x";
constexpr const char *A_WEIGHTS_Y        = "weights.y";

void setup_kmeans_clustering2_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_FEATURE_1);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_FEATURE_2);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  add_seed(node, A_SEED, "Seed");
  add_int(node, A_NCLUSTERS, "nclusters", 4, 1, 16);
  add_float(node, A_WEIGHTS_X, "weights.x", 1.f, 0.01f, 2.f);
  add_float(node, A_WEIGHTS_Y, "weights.y", 1.f, 0.01f, 2.f);
  add_bool(node, A_NORMALIZE_INPUTS, "normalize_inputs", true);
}

void compute_kmeans_clustering2_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // base noise function
  hmap::VirtualArray *p_in1 = node.get_value_ref<hmap::VirtualArray>(P_FEATURE_1);
  hmap::VirtualArray *p_in2 = node.get_value_ref<hmap::VirtualArray>(P_FEATURE_2);
  hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (p_in1 && p_in2)
  {
    hmap::for_each_tile(
        {p_out, p_in1, p_in2},
        [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out, pa_in1, pa_in2] = unpack<3>(p_arrays);

          if (node.val<bool>(A_NORMALIZE_INPUTS))
          {
            hmap::remap(*pa_in1);
            hmap::remap(*pa_in2);
          }

          glm::vec2 weights = {node.val<float>(A_WEIGHTS_X),
                               node.val<float>(A_WEIGHTS_Y)};

          *pa_out = hmap::kmeans_clustering2(*pa_in1,
                                             *pa_in2,
                                             node.val<int>(A_NCLUSTERS),
                                             nullptr, // scoring_arrays,
                                             nullptr, // agg scoring
                                             weights,
                                             node.val<int>(A_SEED));
        },
        node.cfg().cm_single_array);
  }
}

} // namespace hesiod
