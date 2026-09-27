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
        {p_in1, p_in2},
        {p_out},
        [&node](std::vector<const hmap::Array *> p_arrays_in,
                std::vector<hmap::Array *>       p_arrays_out,
                const hmap::TileRegion &)
        {
          auto [pa_in1, pa_in2] = unpack<2>(p_arrays_in);
          auto [pa_out]         = unpack<1>(p_arrays_out);

          hmap::Array        in1_copy;
          hmap::Array        in2_copy;
          const hmap::Array *p_in1_final = pa_in1;
          const hmap::Array *p_in2_final = pa_in2;

          if (node.val<bool>(A_NORMALIZE_INPUTS))
          {
            in1_copy = *pa_in1;
            in2_copy = *pa_in2;
            hmap::remap(in1_copy);
            hmap::remap(in2_copy);
            p_in1_final = &in1_copy;
            p_in2_final = &in2_copy;
          }

          glm::vec2 weights = {node.val<float>(A_WEIGHTS_X),
                               node.val<float>(A_WEIGHTS_Y)};

          *pa_out = hmap::kmeans_clustering2(*p_in1_final,
                                             *p_in2_final,
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
