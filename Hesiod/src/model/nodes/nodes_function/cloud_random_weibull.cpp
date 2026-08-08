/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "meta/metadata/keys.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_CLOUD = "cloud";

constexpr const char *A_DISTANCE_MIN = "distance_min";
constexpr const char *A_K            = "k";
constexpr const char *A_LAMBDA       = "lambda";
constexpr const char *A_REMAP        = "remap";
constexpr const char *A_SEED         = "seed";

void setup_cloud_random_weibull_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Cloud>(gnode::PortType::OUT, P_CLOUD);

  // attribute(s)
  add_float(node, A_DISTANCE_MIN, "distance_min", 0.01f, 0.001f, 0.2f);
  add_float(node, A_LAMBDA, "lambda", 0.1f, 0.001f, 1.f);
  add_float(node, A_K, "k", 1.5f, 0.01f, 4.f);
  add_seed(node, A_SEED, "Seed");
  add_range(node, A_REMAP, "remap", {0.f, 1.f}, -1.f, 2.f, true);
}

void compute_cloud_random_weibull_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Cloud *p_out = node.get_value_ref<hmap::Cloud>(P_CLOUD);

  *p_out = hmap::random_cloud_distance_weibull(node.val<float>(A_DISTANCE_MIN),
                                               node.val<float>(A_LAMBDA),
                                               node.val<float>(A_K),
                                               node.val<int>(A_SEED));

  if (node.metadata_val<bool>(A_REMAP, meta::keys::ui::active))
  {
    glm::vec2 range = node.val<glm::vec2>(A_REMAP);
    p_out->remap_values(range.x, range.y);
  }
}

} // namespace hesiod
