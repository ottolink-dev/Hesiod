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
constexpr const char *P_IN  = "input";
constexpr const char *P_OUT = "output";

constexpr const char *A_DV   = "dv";
constexpr const char *A_DX   = "dx";
constexpr const char *A_DY   = "dy";
constexpr const char *A_SEED = "seed";

void setup_path_shuffle_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Path>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::Path>(gnode::PortType::OUT, P_OUT);

  // attribute(s)
  add_float(node, A_DX, "dx", 0.f, -0.5f, 0.5f);
  add_float(node, A_DY, "dy", 0.f, -0.5f, 0.5f);
  add_float(node, A_DV, "dv", 0.f, -0.5f, 0.5f);
  add_seed(node, A_SEED, "Seed");
}

void compute_path_shuffle_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Path *p_in = node.get_value_ref<hmap::Path>(P_IN);

  if (p_in)
  {
    hmap::Path *p_out = node.get_value_ref<hmap::Path>(P_OUT);

    // copy the input heightmap
    *p_out = *p_in;

    if (p_in->size() > 0)
      p_out->shuffle(node.val<float>(A_DX),
                     node.val<float>(A_DY),
                     node.val<int>(A_SEED),
                     node.val<float>(A_DV));
  }
}

} // namespace hesiod
