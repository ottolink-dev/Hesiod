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

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_INPUT  = "input";
constexpr const char *P_OUTPUT = "output";

constexpr const char *A_ITERATIONS   = "iterations";
constexpr const char *A_SEED         = "seed";
constexpr const char *A_SIGMA        = "sigma";
constexpr const char *A_ORIENTATION  = "orientation";
constexpr const char *A_PERSISTENCE  = "persistence";
constexpr const char *A_REMOVE_LOOPS = "remove_loops";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_path_fractalize_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Path>(gnode::PortType::IN, P_INPUT);
  node.add_port<hmap::Path>(gnode::PortType::OUT, P_OUTPUT);

  // attribute(s)
  add_int(node, A_ITERATIONS, "Iterations", 4, 1, 10);
  add_seed(node, A_SEED, "Random Seed");
  add_float(node, A_SIGMA, "Sigma", 0.3f, 0.f, 1.f);
  add_int(node, A_ORIENTATION, "Orientation", 0, 0, 1);
  add_float(node, A_PERSISTENCE, "Persistence", 1.f, 0.01f, 4.f);
  add_bool(node, A_REMOVE_LOOPS, "Remove Geometric Loops", false);
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_path_fractalize_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Path *p_in  = node.get_value_ref<hmap::Path>(P_INPUT);
  hmap::Path *p_out = node.get_value_ref<hmap::Path>(P_OUTPUT);

  if (!p_in || p_in->size() < 2)
    return;

  // --- Parameters wrapper

  const auto params = [&node]()
  {
    struct P
    {
      int   iterations;
      uint  seed;
      float sigma;
      int   orientation;
      float persistence;
      bool  remove_loops;
    };

    return P{.iterations   = node.val<int>(A_ITERATIONS),
             .seed         = uint(node.val<int>(A_SEED)),
             .sigma        = node.val<float>(A_SIGMA),
             .orientation  = node.val<int>(A_ORIENTATION),
             .persistence  = node.val<float>(A_PERSISTENCE),
             .remove_loops = node.val<bool>(A_REMOVE_LOOPS)};
  }();

  // --- Apply fractalize

  *p_out = hmap::fractalize(*p_in,
                            params.iterations,
                            params.seed,
                            params.sigma,
                            params.orientation,
                            params.persistence);

  if (params.remove_loops)
    *p_out = hmap::remove_geometric_loops(*p_out);
}

} // namespace hesiod
