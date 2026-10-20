/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/landscape/field_hmap.hpp"
#include "hesiod/model/nodes/landscape/mountain.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_ENVELOPE = "envelope";
constexpr const char *P_OUT      = "output";

constexpr const char *A_STYLE          = "style";
constexpr const char *A_SCALE          = "scale";
constexpr const char *A_HEIGHT         = "height";
constexpr const char *A_BULK           = "bulk";
constexpr const char *A_REDUCE_DETAILS = "reduce_details";
constexpr const char *A_SEED           = "seed";
constexpr const char *A_CENTER         = "center";
constexpr const char *A_WORLD_WIDTH    = "world_width";
constexpr const char *A_WORLD_HEIGHT   = "world_height";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_mountain_massif_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ENVELOPE);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s), the enum values are serialized: append only, never renumber
  // clang-format off
  node.set_current_category("Mountain");
  add_enum(node, A_STYLE, "Style", {{0, "Basic"}, {1, "Eroded"}, {2, "Old"}, {3, "Alpine"}, {4, "Strata"}}, 1);
  add_float(node, A_SCALE, "Scale", 0.5f, 0.01f, 1.f);
  add_float(node, A_HEIGHT, "Height", 1.25f, 0.f, 3.f);
  add_enum(node, A_BULK, "Bulk", {{0, "Low"}, {1, "Medium"}, {2, "High"}}, 1);
  add_bool(node, A_REDUCE_DETAILS, "Reduce Details", false);
  add_seed(node, A_SEED, "Seed");
  add_xy(node, A_CENTER, "Position");

  node.set_current_category("Advanced");
  add_float(node, A_WORLD_WIDTH, "Terrain Width", 5000.f, 100.f, 100000.f, "{:.0f} m", true);
  add_float(node, A_WORLD_HEIGHT, "Terrain Height", 2500.f, 10.f, 20000.f, "{:.0f} m", true);
  // clang-format on

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = false, .remap_active_state = true});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_mountain_massif_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_env = node.get_value_ref<hmap::VirtualArray>(P_ENVELOPE);
  hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_out)
    return;

  const glm::vec2 center = node.val<glm::vec2>(A_CENTER);

  landscape::MountainParams params;
  params.style  = landscape::MountainStyle(std::clamp(node.val<int>(A_STYLE), 0, 4));
  params.scale  = node.val<float>(A_SCALE);
  params.height = node.val<float>(A_HEIGHT);
  params.bulk   = landscape::MountainBulk(std::clamp(node.val<int>(A_BULK), 0, 2));
  params.reduce_details = node.val<bool>(A_REDUCE_DETAILS);
  params.seed           = node.val<int>(A_SEED);
  params.x              = center.x;
  params.y              = center.y;
  params.world_width    = node.val<float>(A_WORLD_WIDTH);
  params.world_height   = node.val<float>(A_WORLD_HEIGHT);

  // whole-map generator (erosion and normalizations are global), computed on
  // a power-of-two grid and resampled to the graph shape
  const int        n     = landscape::working_size(p_out->shape);
  landscape::Field field = landscape::mountain(n, params);

  p_out->from_array(landscape::to_array(std::move(field), p_out->shape),
                    node.cfg().cm_cpu);

  // post-process
  post_apply_enveloppe(node, *p_out, p_env);
  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
