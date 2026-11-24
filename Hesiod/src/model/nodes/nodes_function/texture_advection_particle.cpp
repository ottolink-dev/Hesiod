/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <fstream>

#include "highmap/filters.hpp"
#include "highmap/transform.hpp"
#include "highmap/virtual_array/virtual_texture.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_ADVECTION_MASK = "advection_mask";
constexpr const char *P_ELEVATION      = "elevation";
constexpr const char *P_IN             = "input";
constexpr const char *P_MASK           = "mask";
constexpr const char *P_TEXTURE        = "texture";

constexpr const char *A_ADVECTION_LENGTH     = "advection_length";
constexpr const char *A_INERTIA              = "inertia";
constexpr const char *A_ITERATIONS           = "iterations";
constexpr const char *A_LEVELS               = "levels";
constexpr const char *A_MIX                  = "mix";
constexpr const char *A_PARTICLE_DENSITY     = "particle_density";
constexpr const char *A_POST_FILTERING       = "post_filtering";
constexpr const char *A_POST_FILTERING_SIGMA = "post_filtering_sigma";
constexpr const char *A_REVERSE              = "reverse";
constexpr const char *A_SEED                 = "seed";
constexpr const char *A_VALUE_PERSISTENCE    = "value_persistence";
constexpr const char *A_WARP                 = "warp";

constexpr const char *G_SINGLE_SCALE = "Single-Scale";
constexpr const char *G_MULTISCALE   = "Multiscale";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

static void setup_common_advection_attributes(BaseNode &node)
{
  node.set_current_category("Advection");
  add_float(node, A_ADVECTION_LENGTH, "advection_length", 0.1f, 0.f, 0.2f);
  add_float(node, A_VALUE_PERSISTENCE, "value_persistence", 0.99f, 0.8f, 1.f);
  add_float(node, A_INERTIA, "inertia", 0.f, 0.f, 1.f);
  add_bool(node, A_REVERSE, "reverse", false);

  node.set_current_category("Post-Processing");
  add_bool(node, A_POST_FILTERING, "post_filtering", false);
  add_float(node, A_POST_FILTERING_SIGMA, "post_filtering_sigma", 0.07f, 0.f, 0.125f);

  setup_pre_process_mask_attributes(node);
}

void setup_texture_advection_particle_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ELEVATION);
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ADVECTION_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualTexture>(gnode::PortType::OUT, P_TEXTURE, CONFIG_TEX(node));

  // Group: Multiscale
  {
    node.set_current_group(G_MULTISCALE);

    node.set_current_category("Simulation");
    add_seed(node, A_SEED, "Seed");
    add_int(node, A_LEVELS, "levels", 4, 1, 6);
    add_float(node, A_MIX, "mix", 1.f, 0.f, 1.f);
    add_float(node, A_WARP, "warp", 1.f, 0.f, 2.f);
    add_float(node, A_PARTICLE_DENSITY, "particle_density", 0.1f, 0.f, 1.f);

    setup_common_advection_attributes(node);
  }

  // Group: Single-Scale
  {
    node.set_current_group(G_SINGLE_SCALE);

    node.set_current_category("Simulation");
    add_seed(node, A_SEED, "Seed");
    add_float(node, A_PARTICLE_DENSITY, "particle_density", 0.1f, 0.f, 1.f);
    add_int(node, A_ITERATIONS, "iterations", 5, 1, 100);

    setup_common_advection_attributes(node);
  }
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_texture_advection_particle_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  const auto *p_z              = node.get_value_ref<hmap::VirtualArray>(P_ELEVATION);
  const auto *p_tex            = node.get_value_ref<hmap::VirtualTexture>(P_IN);
  const auto *p_advection_mask = node.get_value_ref<hmap::VirtualArray>(P_ADVECTION_MASK);
  const auto *p_mask           = node.get_value_ref<hmap::VirtualArray>(P_MASK);
  auto       *p_out            = node.get_value_ref<hmap::VirtualTexture>(P_TEXTURE);

  if (!p_z || !p_tex)
    return;

  const std::string current_group = node.get_meta_group()
                                        .current_container_name()
                                        .value_or(G_SINGLE_SCALE);

  const uint  seed             = uint(node.val<int>(A_SEED));
  const float particle_density = node.val<float>(A_PARTICLE_DENSITY);

  const float advection_length     = node.val<float>(A_ADVECTION_LENGTH);
  const float value_persistence    = node.val<float>(A_VALUE_PERSISTENCE);
  const float inertia              = node.val<float>(A_INERTIA);
  const bool  reverse              = node.val<bool>(A_REVERSE);
  const bool  post_filtering       = node.val<bool>(A_POST_FILTERING);
  const float post_filtering_sigma = node.val<float>(A_POST_FILTERING_SIGMA);

  // prepare mask
  hmap::VirtualArray mask_default = pre_process_mask(node, p_mask, *p_z);
  if (!mask_default.empty())
    p_mask = &mask_default;

  // build list of data pointers
  std::vector<const hmap::VirtualArray *> ptrs_in = {p_z, p_advection_mask, p_mask};

  for (auto ptr : p_tex->channels_ptr())
    ptrs_in.push_back(ptr);

  std::vector<hmap::VirtualArray *> ptrs_out = p_out->channels_ptr();

  auto lambda = [&](std::vector<const hmap::Array *> p_arrays_in,
                    std::vector<hmap::Array *>       p_arrays_out,
                    const hmap::TileRegion          &region)
  {
    // 3 scalars + 4 channels
    auto [pa_z, pa_advection_mask, pa_mask, pa_r, pa_g, pa_b, pa_a] = unpack<7>(
        p_arrays_in);
    auto [pa_out_r, pa_out_g, pa_out_b, pa_out_a] = unpack<4>(p_arrays_out);

    std::vector<hmap::Array> advected;

    std::vector<const hmap::Array *> in_channels = {pa_r, pa_g, pa_b, pa_a};

    if (current_group == G_MULTISCALE)
    {
      const int        nlevels = std::clamp(node.val<int>(A_LEVELS), 1, 6);
      const float      mix     = node.val<float>(A_MIX);
      const float      warp    = node.val<float>(A_WARP);
      std::vector<int> steps_per_level(nlevels);
      for (int i = 0; i < nlevels; ++i)
        steps_per_level[i] = 1 << (nlevels - 1 - i);

      advected = hmap::gpu::advection_particle_multiscale(*pa_z,
                                                          in_channels,
                                                          seed,
                                                          steps_per_level,
                                                          particle_density,
                                                          advection_length,
                                                          value_persistence,
                                                          inertia,
                                                          mix,
                                                          warp,
                                                          reverse,
                                                          post_filtering,
                                                          post_filtering_sigma,
                                                          pa_advection_mask,
                                                          pa_mask);
    }
    else
    {
      const int nparticles = int(particle_density * region.shape.x * region.shape.y);

      advected = hmap::gpu::advection_particle(*pa_z,
                                               in_channels,
                                               node.val<int>(A_ITERATIONS),
                                               nparticles,
                                               seed,
                                               reverse,
                                               post_filtering,
                                               post_filtering_sigma,
                                               advection_length,
                                               value_persistence,
                                               inertia,
                                               pa_advection_mask,
                                               pa_mask);
    }

    if (advected.size() >= 4)
    {
      *pa_out_r = std::move(advected[0]);
      *pa_out_g = std::move(advected[1]);
      *pa_out_b = std::move(advected[2]);
      *pa_out_a = std::move(advected[3]);
    }
  };

  hmap::for_each_tile(ptrs_in, ptrs_out, lambda, node.cfg().cm_gpu);
}

} // namespace hesiod
