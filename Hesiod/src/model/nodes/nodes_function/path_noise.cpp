/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
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

constexpr const char *P_INPUT  = "input";
constexpr const char *P_OUTPUT = "output";

// Group: Fractalize
constexpr const char *G_FRACTALIZE   = "Fractalize";
constexpr const char *A_ITERATIONS   = "iterations";
constexpr const char *A_SEED         = "seed";
constexpr const char *A_SIGMA        = "sigma";
constexpr const char *A_ORIENTATION  = "orientation";
constexpr const char *A_PERSISTENCE  = "persistence";
constexpr const char *A_REMOVE_LOOPS = "remove_loops";

// Group: Squiggle
constexpr const char *G_SQUIGGLE     = "Squiggle";
constexpr const char *A_HEIGHT_RATIO = "height_ratio";

// Group: Meanderize
constexpr const char *G_MEANDERIZE     = "Meanderize";
constexpr const char *A_RATIO          = "ratio";
constexpr const char *A_NOISE_RATIO    = "noise_ratio";
constexpr const char *A_EDGE_DIVISIONS = "edge_divisions";

// Group: Shuffle
constexpr const char *G_SHUFFLE = "Shuffle";
constexpr const char *A_DV      = "dv";
constexpr const char *A_DX      = "dx";
constexpr const char *A_DY      = "dy";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_path_noise_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Path>(gnode::PortType::IN, P_INPUT);
  node.add_port<hmap::Path>(gnode::PortType::OUT, P_OUTPUT);

  // Group: Fractalize
  {
    node.set_current_group(G_FRACTALIZE);

    node.set_current_category("Main Parameters");

    add_int(node, A_ITERATIONS, "Iterations", 4, 1, 10);
    add_seed(node, A_SEED, "Random Seed");
    add_float(node, A_SIGMA, "Sigma", 0.3f, 0.f, 1.f);
    add_int(node, A_ORIENTATION, "Orientation", 0, 0, 1);
    add_float(node, A_PERSISTENCE, "Persistence", 1.f, 0.01f, 4.f);

    node.set_current_category("Post-Process");

    add_bool(node, A_REMOVE_LOOPS, "Remove Geometric Loops", false);
  }

  // Group: Squiggle
  {
    node.set_current_group(G_SQUIGGLE);

    node.set_current_category("Main Parameters");

    add_int(node, A_ITERATIONS, "Iterations", 4, 1, 10);
    add_seed(node, A_SEED, "Random Seed");
    add_float(node, A_HEIGHT_RATIO, "Height Ratio", 0.5f, 0.f, 1.f);
    add_int(node, A_ORIENTATION, "Orientation", 0, -1, 1);

    node.set_current_category("Post-Process");

    add_bool(node, A_REMOVE_LOOPS, "Remove Geometric Loops", false);
  }

  // Group: Meanderize
  {
    node.set_current_group(G_MEANDERIZE);

    node.set_current_category("Main Parameters");

    add_float(node, A_RATIO, "Meander Ratio", 0.2f, 0.f, 1.f);
    add_float(node, A_NOISE_RATIO, "Noise Ratio", 0.1f, 0.f, 1.f);
    add_seed(node, A_SEED, "Seed");
    add_int(node, A_ITERATIONS, "Solver Iterations", 2, 1, 8);
    add_int(node, A_EDGE_DIVISIONS, "Edge Divisions", 10, 1, 32);

    node.set_current_category("Post-Process");

    add_bool(node, A_REMOVE_LOOPS, "Remove Geometric Loops", false);
  }

  // Group: Shuffle
  {
    node.set_current_group(G_SHUFFLE);

    add_float(node, A_DX, "dx", 0.f, -0.5f, 0.5f);
    add_float(node, A_DY, "dy", 0.f, -0.5f, 0.5f);
    add_float(node, A_DV, "dv", 0.f, -0.5f, 0.5f);
    add_seed(node, A_SEED, "Seed");
  }

  // Reset active group to first
  node.set_current_group(G_FRACTALIZE);
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_path_noise_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Path *p_in  = node.get_value_ref<hmap::Path>(P_INPUT);
  hmap::Path *p_out = node.get_value_ref<hmap::Path>(P_OUTPUT);

  if (!p_in || p_in->empty())
    return;

  const std::string current_group = node.get_meta_group()
                                        .current_container_name()
                                        .value_or(G_FRACTALIZE);

  Logger::log()->trace("compute_path_noise_node: current_group {}", current_group);

  if (current_group == G_FRACTALIZE)
  {
    if (p_in->size() < 2)
      return;

    const auto iterations   = node.val<int>(A_ITERATIONS);
    const auto seed         = uint(node.val<int>(A_SEED));
    const auto sigma        = node.val<float>(A_SIGMA);
    const auto orientation  = node.val<int>(A_ORIENTATION);
    const auto persistence  = node.val<float>(A_PERSISTENCE);
    const auto remove_loops = node.val<bool>(A_REMOVE_LOOPS);

    *p_out = hmap::fractalize(*p_in,
                              iterations,
                              seed,
                              sigma,
                              orientation,
                              persistence,
                              /* p_control_field */ nullptr,
                              /* bbox */ glm::vec4{0.f, 1.f, 0.f, 1.f},
                              /* bounded */ false);

    if (remove_loops)
      *p_out = hmap::remove_geometric_loops(*p_out);
  }
  else if (current_group == G_SQUIGGLE)
  {
    if (p_in->size() < 2)
      return;

    const auto iterations   = node.val<int>(A_ITERATIONS);
    const auto seed         = uint(node.val<int>(A_SEED));
    const auto height_ratio = node.val<float>(A_HEIGHT_RATIO);
    const auto orientation  = node.val<int>(A_ORIENTATION);
    const auto remove_loops = node.val<bool>(A_REMOVE_LOOPS);

    *p_out = hmap::squiggle(*p_in,
                            iterations,
                            seed,
                            height_ratio,
                            orientation,
                            /* p_weights */ nullptr,
                            /* p_mask */ nullptr,
                            /* bbox */ glm::vec4{0.f, 1.f, 0.f, 1.f});

    if (remove_loops)
      *p_out = hmap::remove_geometric_loops(*p_out);
  }
  else if (current_group == G_MEANDERIZE)
  {
    if (p_in->size() < 2)
      return;

    const auto ratio          = node.val<float>(A_RATIO);
    const auto noise_ratio    = node.val<float>(A_NOISE_RATIO);
    const auto seed           = uint(node.val<int>(A_SEED));
    const auto iterations     = node.val<int>(A_ITERATIONS);
    const auto edge_divisions = node.val<int>(A_EDGE_DIVISIONS);
    const auto remove_loops   = node.val<bool>(A_REMOVE_LOOPS);

    *p_out = hmap::meanderize(*p_in,
                              ratio,
                              noise_ratio,
                              seed,
                              iterations,
                              edge_divisions);

    if (remove_loops)
      *p_out = hmap::remove_geometric_loops(*p_out);
  }
  else if (current_group == G_SHUFFLE)
  {
    *p_out = *p_in;
    if (p_in->size() > 0)
    {
      p_out->shuffle(node.val<float>(A_DX),
                     node.val<float>(A_DY),
                     node.val<int>(A_SEED),
                     node.val<float>(A_DV));
    }
  }
  else
  {
    Logger::log()->error("compute_path_noise_node: group {} not implemented",
                         current_group);
  }
}

} // namespace hesiod
