/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/geometry/path.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

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
  node.add_attr<IntAttribute>(A_ITERATIONS, "Iterations", 4, 1, 10);
  node.add_attr<SeedAttribute>(A_SEED, "Random Seed");
  node.add_attr<FloatAttribute>(A_SIGMA, "Sigma", 0.3f, 0.f, 1.f);
  node.add_attr<IntAttribute>(A_ORIENTATION, "Orientation", 0, 0, 1);
  node.add_attr<FloatAttribute>(A_PERSISTENCE, "Persistence", 1.f, 0.01f, 4.f);
  node.add_attr<BoolAttribute>(A_REMOVE_LOOPS, "Remove Geometric Loops", false);

  // attribute(s) order
  node.set_attr_ordered_key({"_GROUPBOX_BEGIN_Fractal Parameters",
                             A_ITERATIONS,
                             A_SEED,
                             A_SIGMA,
                             A_ORIENTATION,
                             A_PERSISTENCE,
                             A_REMOVE_LOOPS,
                             "_GROUPBOX_END_"});
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

    return P{.iterations   = node.get_attr<IntAttribute>(A_ITERATIONS),
             .seed         = node.get_attr<SeedAttribute>(A_SEED),
             .sigma        = node.get_attr<FloatAttribute>(A_SIGMA),
             .orientation  = node.get_attr<IntAttribute>(A_ORIENTATION),
             .persistence  = node.get_attr<FloatAttribute>(A_PERSISTENCE),
             .remove_loops = node.get_attr<BoolAttribute>(A_REMOVE_LOOPS)};
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
