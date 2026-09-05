/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/primitives.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_OUT = "output";

constexpr const char *A_CENTER       = "center";
constexpr const char *A_DISPLACEMENT = "displacement";
constexpr const char *A_KW           = "kw";
constexpr const char *A_LACUNARITY   = "lacunarity";
constexpr const char *A_NOISE_TYPE   = "noise_type";
constexpr const char *A_OCTAVES      = "octaves";
constexpr const char *A_PERSISTENCE  = "persistence";
constexpr const char *A_RADIUS       = "radius";
constexpr const char *A_SEED         = "seed";
constexpr const char *A_WEIGHT       = "weight";

void setup_island_land_mask_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  add_float(node, A_RADIUS, "radius", 0.2f, 0.f, 1.f);
  add_seed(node, A_SEED, "Seed");
  add_float(node, A_DISPLACEMENT, "displacement", 0.4f, 0.f, FLT_MAX);
  add_enum(node, A_NOISE_TYPE, "Type", enum_mappings.noise_type_map);
  add_float(node, A_KW, "kw", 4.f, 0.f, FLT_MAX);
  add_int(node, A_OCTAVES, "Octaves", 8, 0, 32);
  add_float(node, A_WEIGHT, "Weight", 0.7f, 0.f, 1.f);
  add_float(node, A_PERSISTENCE, "Persistence", 1.f, 0.f, 1.f);
  add_float(node, A_LACUNARITY, "Lacunarity", 2.f, 0.01f, 4.f);
  add_xy(node, A_CENTER, "center");

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = true});
}

void compute_island_land_mask_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // base noise function
  hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  hmap::for_each_tile(
      {p_out},
      [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &region)
      {
        auto [pa_out] = unpack<1>(p_arrays);

        *pa_out = hmap::island_land_mask(region.shape,
                                         node.val<float>(A_RADIUS),
                                         node.val<int>(A_SEED),
                                         node.val<float>(A_DISPLACEMENT),
                                         (hmap::NoiseType)node.val<int>(A_NOISE_TYPE),
                                         node.val<float>(A_KW),
                                         node.val<int>(A_OCTAVES),
                                         node.val<float>(A_WEIGHT),
                                         node.val<float>(A_PERSISTENCE),
                                         node.val<float>(A_LACUNARITY),
                                         node.val<glm::vec2>(A_CENTER),
                                         region.bbox);
      },
      node.cfg().cm_cpu);

  // post-process
  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
