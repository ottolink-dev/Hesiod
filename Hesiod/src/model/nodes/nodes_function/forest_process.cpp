/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/flora/forest.hpp"
#include "highmap/scatter/scatter_field.hpp"
#include "highmap/virtual_array/virtual_array.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_INPUT        = "input";
constexpr const char *P_OUTPUT       = "output";
constexpr const char *P_ELEVATION    = "elevation";
constexpr const char *P_DENSITY_MASK = "density_mask";

constexpr const char *A_SCALE_RADIUS             = "scale_radius";
constexpr const char *A_PRUNE_COLLISIONS         = "prune_collisions";
constexpr const char *A_PRUNE_DENSITY            = "prune_density";
constexpr const char *A_REINFORCE_CLASS_CLUSTERS = "reinforce_class_clusters";
constexpr const char *A_SEED                     = "seed";

void setup_forest_process_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::Forest>(gnode::PortType::IN, P_INPUT);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ELEVATION);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DENSITY_MASK);
  node.add_port<hmap::Forest>(gnode::PortType::OUT, P_OUTPUT);

  // --- Attributes

  add_float(node, A_SCALE_RADIUS, "Scale Radius", 1.0f, 0.0001f, 100.0f, "{:.2e}", true);
  add_bool(node, A_PRUNE_COLLISIONS, "Prune Collisions", false);
  add_float(node, A_PRUNE_DENSITY, "Prune Density", 1.0f, 0.0f, 1.0f);
  add_int(node, A_REINFORCE_CLASS_CLUSTERS, "Reinforce Class Clusters", 0, 0, 16);
  add_seed(node, A_SEED, "Seed");
}

void compute_forest_process_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_in_forest    = node.get_value_ref<hmap::Forest>(P_INPUT);
  auto *p_out_forest   = node.get_value_ref<hmap::Forest>(P_OUTPUT);
  auto *p_elevation    = node.get_value_ref<hmap::VirtualArray>(P_ELEVATION);
  auto *p_density_mask = node.get_value_ref<hmap::VirtualArray>(P_DENSITY_MASK);

  if (!p_in_forest || p_in_forest->empty())
    return;

  *p_out_forest = *p_in_forest;

  // --- Params

  const auto scale_radius             = node.val<float>(A_SCALE_RADIUS);
  const auto prune_collisions         = node.val<bool>(A_PRUNE_COLLISIONS);
  const auto prune_density_ratio      = node.val<float>(A_PRUNE_DENSITY);
  const auto reinforce_class_clusters = node.val<int>(A_REINFORCE_CLASS_CLUSTERS);
  const auto base_seed                = static_cast<uint32_t>(node.val<int>(A_SEED));

  // --- Set elevation from heightmap (optional input)

  if (p_elevation)
  {
    hmap::for_each_tile(
        {p_elevation},
        {},
        [&](const std::vector<const hmap::Array *> &p_arrays_in,
            std::vector<hmap::Array *> &,
            const hmap::TileRegion &region)
        {
          const auto [p_elev_tile] = unpack<1>(p_arrays_in);
          p_out_forest->set_elevation_from_terrain(*p_elev_tile, region.bbox);
        },
        node.cfg().cm_cpu);
  }

  // --- Scale radius (default 1.0, nothing)

  if (std::abs(scale_radius - 1.0f) > 1e-6f)
  {
    for (auto &item : *p_out_forest)
      item.radius *= scale_radius;
  }

  // --- Prune density

  if (prune_density_ratio == 0.f)
  {
    *p_out_forest = hmap::Forest{};
  }
  else if (prune_density_ratio < 1.0f && !p_out_forest->empty())
  {
    hmap::VirtualArray  dummy_mask;
    hmap::VirtualArray *p_mask_va = p_density_mask ? p_density_mask : &dummy_mask;

    if (!p_density_mask)
    {
      dummy_mask = hmap::VirtualArray(CONFIG(node));
      dummy_mask.fill(1.0f, node.cfg().cm_cpu);
    }

    // sequential is mandatory to avoid race condition
    auto cm = node.cfg().cm_cpu;
    cm.mode = hmap::ForEachMode::VA_SEQUENTIAL;

    hmap::for_each_tile(
        {p_mask_va},
        {},
        [&](const std::vector<const hmap::Array *> &p_arrays_in,
            std::vector<hmap::Array *> &,
            const hmap::TileRegion &region)
        {
          const auto [pa_mask]     = unpack<1>(p_arrays_in);
          const uint32_t tile_seed = base_seed + region.key.hash();

          p_out_forest->prune_density(*pa_mask,
                                      prune_density_ratio,
                                      tile_seed,
                                      region.inner_bbox());
        },
        cm);
  }

  // --- Prune collisions (outside tile loop)

  if (prune_collisions && !p_out_forest->empty())
  {
    p_out_forest->prune_collisions();
  }

  // --- Reinforce class clusters

  if (reinforce_class_clusters > 0 && !p_out_forest->empty())
  {
    p_out_forest->reinforce_class_clusters(static_cast<size_t>(reinforce_class_clusters));
  }
}

} // namespace hesiod
