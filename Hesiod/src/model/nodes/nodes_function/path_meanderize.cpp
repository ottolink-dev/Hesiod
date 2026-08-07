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

constexpr const char *A_RATIO          = "ratio";
constexpr const char *A_NOISE_RATIO    = "noise_ratio";
constexpr const char *A_SEED           = "seed";
constexpr const char *A_ITERATIONS     = "iterations";
constexpr const char *A_EDGE_DIVISIONS = "edge_divisions";
constexpr const char *A_REMOVE_LOOPS   = "remove_loops";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_path_meanderize_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Path>(gnode::PortType::IN, P_INPUT);
  node.add_port<hmap::Path>(gnode::PortType::OUT, P_OUTPUT);

  // attribute(s)
  add_float(node, A_RATIO, "Meander Ratio", 0.2f, 0.f, 1.f);
  add_float(node, A_NOISE_RATIO, "Noise Ratio", 0.1f, 0.f, 1.f);
  add_seed(node, A_SEED, "Seed");
  add_int(node, A_ITERATIONS, "Solver Iterations", 2, 1, 8);
  add_int(node, A_EDGE_DIVISIONS, "Edge Divisions", 10, 1, 32);
  add_bool(node, A_REMOVE_LOOPS, "Remove Geometric Loops", false);
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_path_meanderize_node(BaseNode &node)
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
      float ratio;
      float noise_ratio;
      uint  seed;
      int   iterations;
      int   edge_divisions;
      bool  remove_loops;
    };
    return P{
        .ratio          = node.val<float>(A_RATIO),
        .noise_ratio    = node.val<float>(A_NOISE_RATIO),
        .seed           = node.val<int>(A_SEED),
        .iterations     = node.val<int>(A_ITERATIONS),
        .edge_divisions = node.val<int>(A_EDGE_DIVISIONS),
        .remove_loops   = node.val<bool>(A_REMOVE_LOOPS),
    };
  }();

  // --- Apply meanderize

  *p_out = hmap::meanderize(*p_in,
                            params.ratio,
                            params.noise_ratio,
                            params.seed,
                            params.iterations,
                            params.edge_divisions);

  if (params.remove_loops)
    *p_out = hmap::remove_geometric_loops(*p_out);
}

} // namespace hesiod
