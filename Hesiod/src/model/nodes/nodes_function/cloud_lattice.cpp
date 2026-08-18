/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/geometry/grids.hpp"

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

constexpr const char *A_JITTER_RATIO  = "jitter_ratio";
constexpr const char *A_NPOINTS       = "npoints";
constexpr const char *A_REMAP         = "remap";
constexpr const char *A_SEED          = "seed";
constexpr const char *A_STAGGER_RATIO = "stagger_ratio";

void setup_cloud_lattice_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Cloud>(gnode::PortType::OUT, P_CLOUD);

  // attribute(s)
  glm::vec2 default_value = {0.1f, 0.1f};
  add_int(node, A_NPOINTS, "npoints", 50, 1, INT_MAX);
  add_wavenumber(node, A_STAGGER_RATIO, "stagger_ratio", default_value, 0.f, 1.f, true);
  add_wavenumber(node, A_JITTER_RATIO, "jitter_ratio", default_value, 0.f, 1.f, true);
  add_seed(node, A_SEED, "Seed");
  add_range(node, A_REMAP, "remap", {0.f, 1.f}, -1.f, 2.f, true);
}

void compute_cloud_lattice_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Cloud *p_out = node.get_value_ref<hmap::Cloud>(P_CLOUD);

  *p_out = hmap::random_cloud_jittered(node.val<int>(A_NPOINTS),
                                       node.val<glm::vec2>(A_JITTER_RATIO),
                                       node.val<glm::vec2>(A_STAGGER_RATIO),
                                       node.val<int>(A_SEED));

  if (node.state_val<bool>(A_REMAP, meta::keys::state::active))
  {
    glm::vec2 range = node.val<glm::vec2>(A_REMAP);
    p_out->remap_values(range.x, range.y);
  }
}

} // namespace hesiod
