/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/hydrology/hydrology.hpp"

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
constexpr const char *P_DX  = "dx";
constexpr const char *P_DY  = "dy";
constexpr const char *P_OUT = "output";

constexpr const char *A_RIVERBED_SLOPE        = "riverbed_slope";
constexpr const char *A_ELEVATION_RATIO       = "elevation_ratio";
constexpr const char *A_DISTANCE_EXPONENT     = "distance_exponent";
constexpr const char *A_UPWARD_PENALIZATION   = "upward_penalization";
constexpr const char *A_VALLEY_AFFINITY       = "valley_affinity";
constexpr const char *A_PATH_SINUOSITY        = "path_sinuosity";
constexpr const char *A_PREFILTER_RADIUS      = "prefilter_radius";
constexpr const char *A_CARVE_RIVERBED        = "carve_riverbed";
constexpr const char *A_SMOOTH_RIVER_BOTTOM   = "smooth_river_bottom";
constexpr const char *A_TALUS_RIVERBANK       = "talus_riverbank";
constexpr const char *A_RIVERBANK_NOISE_RATIO = "riverbank_noise_ratio";
constexpr const char *A_MERGING_RADIUS        = "merging_radius";
constexpr const char *A_SEED                  = "seed";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_flow_fixing_mst_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DX);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DY);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Attributes

  // clang-format off
  node.set_current_category("Riverbed Slope & Pathfinding");
  add_float(node, A_RIVERBED_SLOPE, "Riverbed Slope", 0.01f, 0.f, 0.1f);
  add_float(node, A_ELEVATION_RATIO, "Elevation vs Slope Weight", 0.95f, 0.f, 1.f);
  add_float(node, A_DISTANCE_EXPONENT, "Distance Exponent", 2.f, 0.1f, 4.f);
  add_float(node, A_UPWARD_PENALIZATION, "Upward Penalization", 50.f, 1.f, 1000.f);
  add_float(node, A_VALLEY_AFFINITY, "Valley Affinity", 0.5f, 0.f, 1.f);
  add_float(node, A_PATH_SINUOSITY, "Path Sinuosity", 0.25f, 0.f, 1.f);
  add_float(node, A_PREFILTER_RADIUS, "Prefilter Radius", 0.02f, 0.f, 0.1f);

  node.set_current_category("Riverbank Carving");
  add_bool(node, A_CARVE_RIVERBED, "Carve Riverbed", true);
  add_bool(node, A_SMOOTH_RIVER_BOTTOM, "Smooth River Bottom", true);
  add_float(node, A_TALUS_RIVERBANK, "Riverbank Talus", 0.01f, 0.f, 0.1f);
  add_float(node, A_RIVERBANK_NOISE_RATIO, "Riverbank Noise Ratio", 0.f, 0.f, 1.f);
  add_float(node, A_MERGING_RADIUS, "Merging Radius", 0.02f, 0.f, 0.2f);
  add_seed(node, A_SEED, "Seed");
  // clang-format on

  // --- Attribute(s) order

  setup_default_noise(node, {.noise_amp = 0.05f, .kw = 8.f, .smoothness = 0.2f});
  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_flow_fixing_mst_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  auto *p_in  = node.get_value_ref<hmap::VirtualArray>(P_IN);
  auto *p_dx  = node.get_value_ref<hmap::VirtualArray>(P_DX);
  auto *p_dy  = node.get_value_ref<hmap::VirtualArray>(P_DY);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_in)
    return;

  // --- Parameters

  const auto nx                  = float(p_in->shape.x);
  const auto riverbed_talus      = node.val<float>(A_RIVERBED_SLOPE) / std::max(1.f, nx);
  const auto elevation_ratio     = node.val<float>(A_ELEVATION_RATIO);
  const auto distance_exponent   = node.val<float>(A_DISTANCE_EXPONENT);
  const auto upward_penalization = node.val<float>(A_UPWARD_PENALIZATION);
  const auto valley_affinity     = node.val<float>(A_VALLEY_AFFINITY);
  const auto path_sinuosity      = node.val<float>(A_PATH_SINUOSITY);
  const auto prefilter_ir        = int(node.val<float>(A_PREFILTER_RADIUS) * nx);
  const auto carve_riverbed      = node.val<bool>(A_CARVE_RIVERBED);
  const auto smooth_river_bottom = node.val<bool>(A_SMOOTH_RIVER_BOTTOM);
  const auto talus_riverbank     = node.val<float>(A_TALUS_RIVERBANK);
  const auto seed                = std::uint32_t(node.val<int>(A_SEED));
  const auto riverbank_noise_ratio = node.val<float>(A_RIVERBANK_NOISE_RATIO);
  const auto merging_distance      = node.val<float>(A_MERGING_RADIUS) * nx;

  // --- Prepare default noise

  hmap::VirtualArray noise_default_x(CONFIG(node));
  hmap::VirtualArray noise_default_y(CONFIG(node));
  uint               seed_increment = 0;
  generate_noise(node, p_dx, noise_default_x, ++seed_increment);
  generate_noise(node, p_dy, noise_default_y, ++seed_increment);

  // --- Compute

  hmap::for_each_tile(
      {p_in, p_dx, p_dy},
      {p_out},
      [&](std::vector<const hmap::Array *> p_arrays_in,
          std::vector<hmap::Array *>       p_arrays_out,
          const hmap::TileRegion &)
      {
        auto [pa_in, pa_dx, pa_dy] = unpack<3>(p_arrays_in);
        auto [pa_out]              = unpack<1>(p_arrays_out);

        *pa_out = hmap::flow_fixing_mst(*pa_in,
                                        riverbed_talus,
                                        elevation_ratio,
                                        distance_exponent,
                                        upward_penalization,
                                        valley_affinity,
                                        path_sinuosity,
                                        prefilter_ir,
                                        carve_riverbed,
                                        smooth_river_bottom,
                                        talus_riverbank,
                                        seed,
                                        riverbank_noise_ratio,
                                        merging_distance,
                                        pa_dx,
                                        pa_dy);
      },
      node.cfg().cm_single_array); // forced, not tileable

  // --- Post-process

  post_process_heightmap(node, *p_out, p_in);
}

} // namespace hesiod
