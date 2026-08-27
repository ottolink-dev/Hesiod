/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <cmath>

#include "highmap/geometry/cloud.hpp"
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/primitives.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_DX    = "dx";
constexpr const char *P_DY    = "dy";
constexpr const char *P_CTRL  = "control";
constexpr const char *P_CLOUD = "cloud";
constexpr const char *P_ENV   = "envelope";
constexpr const char *P_OUT   = "output";

constexpr const char *A_RETURN_TYPE = "return_type";
constexpr const char *A_KW          = "kw";
constexpr const char *A_SEED        = "seed";
constexpr const char *A_JITTER_X    = "jitter.x";
constexpr const char *A_JITTER_Y    = "jitter.y";
constexpr const char *A_K_SMOOTHING = "k_smoothing";
constexpr const char *A_EXP_SIGMA   = "exp_sigma";
constexpr const char *A_SQRT_OUTPUT = "sqrt_output";
constexpr const char *A_OCTAVES     = "octaves";
constexpr const char *A_WEIGHT      = "weight";
constexpr const char *A_PERSISTENCE = "persistence";
constexpr const char *A_LACUNARITY  = "lacunarity";
constexpr const char *A_DENSITY     = "density";
constexpr const char *A_ANGLE       = "angle";
constexpr const char *A_ANGLE_SPAN  = "angle_span";
constexpr const char *A_VARIABILITY = "variability";
constexpr const char *A_U           = "u";
constexpr const char *A_V           = "v";

constexpr const char *G_GRID      = "Grid";
constexpr const char *G_LINES     = "Lines";
constexpr const char *G_SCATTERED = "Scattered";
constexpr const char *G_VORONOISE = "Voronoise";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_cellular_noise_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DX);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DY);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_CTRL);
  node.add_port<hmap::Cloud>(gnode::PortType::IN, P_CLOUD);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ENV);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Group 1: Grid (Voronoi / VoronoiFbm)
  {
    node.set_current_group(G_GRID);

    node.set_current_category("Cellular");
    add_enum(node,
             A_RETURN_TYPE,
             "Return Type",
             enum_mappings.voronoi_return_type_map,
             "F1: squared distance to the closest point");
    add_wavenumber(node, A_KW, "Spatial Frequency");
    add_seed(node, A_SEED, "Seed");
    add_float(node, A_JITTER_X, "jitter.x", 1.f, 0.f, 1.f);
    add_float(node, A_JITTER_Y, "jitter.y", 1.f, 0.f, 1.f);
    add_float(node, A_K_SMOOTHING, "k_smoothing", 0.f, 0.f, 1.f);
    add_float(node, A_EXP_SIGMA, "exp_sigma", 0.1f, 0.f, 0.3f);
    add_bool(node, A_SQRT_OUTPUT, "sqrt_output", false);

    node.set_current_category("FBM Layers");
    add_int(node, A_OCTAVES, "Octaves", 1, 1, 32);
    add_float(node, A_WEIGHT, "Weight", 0.7f, 0.f, 1.f);
    add_float(node, A_PERSISTENCE, "Persistence", 0.5f, 0.f, 1.f);
    add_float(node, A_LACUNARITY, "Lacunarity", 2.f, 0.01f, 4.f);

    setup_post_process_heightmap_attributes(
        node,
        {.add_mix = false, .remap_active_state = true});
  }

  // --- Group 2: Lines (Vorolines / VorolinesFbm)
  {
    node.set_current_group(G_LINES);

    node.set_current_category("Cellular");
    add_enum(node,
             A_RETURN_TYPE,
             "Return Type",
             enum_mappings.voronoi_return_type_map,
             "F1: squared distance to the closest point");
    add_float(node, A_DENSITY, "density", 8.f, 0.f, 100.f);
    add_seed(node, A_SEED, "Seed");
    add_float(node, A_K_SMOOTHING, "k_smoothing", 0.f, 0.f, 1.f);
    add_float(node, A_EXP_SIGMA, "exp_sigma", 0.1f, 0.f, 0.1f);
    add_float(node, A_ANGLE, "angle", 0.f, 0.f, 180.f);
    add_float(node, A_ANGLE_SPAN, "angle_span", 90.f, 0.f, 180.f);
    add_bool(node, A_SQRT_OUTPUT, "sqrt_output", false);

    node.set_current_category("FBM Layers");
    add_int(node, A_OCTAVES, "Octaves", 1, 1, 32);
    add_float(node, A_WEIGHT, "Weight", 0.7f, 0.f, 1.f);
    add_float(node, A_PERSISTENCE, "Persistence", 0.5f, 0.f, 1.f);
    add_float(node, A_LACUNARITY, "Lacunarity", 2.f, 0.01f, 4.f);

    setup_post_process_heightmap_attributes(
        node,
        {.add_mix = false, .remap_active_state = true});
  }

  // --- Group 3: Scattered (Vororand)
  {
    node.set_current_group(G_SCATTERED);

    node.set_current_category("Cellular");
    add_enum(node,
             A_RETURN_TYPE,
             "Return Type",
             enum_mappings.voronoi_return_type_map,
             "F1: squared distance to the closest point");
    add_float(node, A_DENSITY, "density", 1.f, 0.f, 100.f);
    add_float(node, A_VARIABILITY, "variability", 1.f, 1.f, 10.f);
    add_seed(node, A_SEED, "Seed");
    add_float(node, A_K_SMOOTHING, "k_smoothing", 0.f, 0.f, 1.f);
    add_float(node, A_EXP_SIGMA, "exp_sigma", 0.1f, 0.f, 0.3f);
    add_bool(node, A_SQRT_OUTPUT, "sqrt_output", false);

    setup_post_process_heightmap_attributes(
        node,
        {.add_mix = false, .remap_active_state = true});
  }

  // --- Group 4: Voronoise (Voronoise)
  {
    node.set_current_group(G_VORONOISE);

    node.set_current_category("Cellular");
    add_wavenumber(node, A_KW, "Spatial Frequency");
    add_seed(node, A_SEED, "Seed");
    add_float(node, A_U, "u", 0.5f, 0.f, 1.f);
    add_float(node, A_V, "v", 0.5f, 0.f, 1.f);

    setup_post_process_heightmap_attributes(
        node,
        {.add_mix = false, .remap_active_state = true});
  }

  // Reset active group to first
  node.set_current_group(G_GRID);
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_cellular_noise_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_dx    = node.get_value_ref<hmap::VirtualArray>(P_DX);
  hmap::VirtualArray *p_dy    = node.get_value_ref<hmap::VirtualArray>(P_DY);
  hmap::VirtualArray *p_ctrl  = node.get_value_ref<hmap::VirtualArray>(P_CTRL);
  hmap::Cloud        *p_cloud = node.get_value_ref<hmap::Cloud>(P_CLOUD);
  hmap::VirtualArray *p_env   = node.get_value_ref<hmap::VirtualArray>(P_ENV);
  hmap::VirtualArray *p_out   = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  const std::string
      current_group = node.get_meta_group().current_container_name().value_or(G_GRID);

  Logger::log()->trace("compute_cellular_noise_node: current_group {}", current_group);

  if (current_group == G_GRID)
  {
    const auto rtype = static_cast<hmap::VoronoiReturnType>(node.val<int>(A_RETURN_TYPE));
    const auto kw    = node.val<glm::vec2>(A_KW);
    const auto seed  = node.val<int>(A_SEED);
    const auto jitter      = glm::vec2(node.val<float>(A_JITTER_X),
                                  node.val<float>(A_JITTER_Y));
    const auto k_smoothing = node.val<float>(A_K_SMOOTHING);
    const auto exp_sigma   = node.val<float>(A_EXP_SIGMA);
    const auto octaves     = node.val<int>(A_OCTAVES);
    const auto weight      = node.val<float>(A_WEIGHT);
    const auto persistence = node.val<float>(A_PERSISTENCE);
    const auto lacunarity  = node.val<float>(A_LACUNARITY);

    if (octaves > 1)
    {
      hmap::for_each_tile(
          {p_out, p_dx, p_dy, p_ctrl},
          [&](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &region)
          {
            auto [pa_out, pa_dx, pa_dy, pa_ctrl] = unpack<4>(p_arrays);
            *pa_out                              = hmap::gpu::voronoi_fbm(region.shape,
                                             kw,
                                             seed,
                                             jitter,
                                             k_smoothing,
                                             exp_sigma,
                                             rtype,
                                             octaves,
                                             weight,
                                             persistence,
                                             lacunarity,
                                             pa_ctrl,
                                             pa_dx,
                                             pa_dy,
                                             region.bbox);
          },
          node.cfg().cm_gpu);
    }
    else
    {
      hmap::for_each_tile(
          {p_out, p_dx, p_dy, p_ctrl},
          [&](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &region)
          {
            auto [pa_out, pa_dx, pa_dy, pa_ctrl] = unpack<4>(p_arrays);
            *pa_out                              = hmap::gpu::voronoi(region.shape,
                                         kw,
                                         seed,
                                         jitter,
                                         k_smoothing,
                                         exp_sigma,
                                         rtype,
                                         pa_ctrl,
                                         pa_dx,
                                         pa_dy,
                                         region.bbox);
          },
          node.cfg().cm_gpu);
    }

    p_out->remap(0.f, 1.f, node.cfg().cm_cpu);

    if (node.val<bool>(A_SQRT_OUTPUT))
    {
      hmap::for_each_tile(
          {p_out},
          [](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
          {
            hmap::Array *pa_out = p_arrays[0];
            *pa_out             = hmap::sqrt(*pa_out);
          },
          node.cfg().cm_cpu);
    }
  }
  else if (current_group == G_LINES)
  {
    const auto rtype = static_cast<hmap::VoronoiReturnType>(node.val<int>(A_RETURN_TYPE));
    const auto density     = node.val<float>(A_DENSITY);
    const auto seed        = node.val<int>(A_SEED);
    const auto k_smoothing = node.val<float>(A_K_SMOOTHING);
    const auto exp_sigma   = node.val<float>(A_EXP_SIGMA);
    const auto angle_rad   = static_cast<float>(M_PI / 180.f) * node.val<float>(A_ANGLE);
    const auto span_rad    = static_cast<float>(M_PI / 180.f) *
                          node.val<float>(A_ANGLE_SPAN);
    const auto octaves     = node.val<int>(A_OCTAVES);
    const auto weight      = node.val<float>(A_WEIGHT);
    const auto persistence = node.val<float>(A_PERSISTENCE);
    const auto lacunarity  = node.val<float>(A_LACUNARITY);

    if (octaves > 1)
    {
      hmap::for_each_tile(
          {p_out, p_dx, p_dy},
          [&](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &region)
          {
            auto [pa_out, pa_dx, pa_dy] = unpack<3>(p_arrays);
            *pa_out                     = hmap::gpu::vorolines_fbm(region.shape,
                                               density,
                                               seed,
                                               k_smoothing,
                                               exp_sigma,
                                               angle_rad,
                                               span_rad,
                                               rtype,
                                               octaves,
                                               weight,
                                               persistence,
                                               lacunarity,
                                               pa_dx,
                                               pa_dy,
                                               region.bbox);
          },
          node.cfg().cm_gpu);
    }
    else
    {
      hmap::for_each_tile(
          {p_out, p_dx, p_dy},
          [&](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &region)
          {
            auto [pa_out, pa_dx, pa_dy] = unpack<3>(p_arrays);
            *pa_out                     = hmap::gpu::vorolines(region.shape,
                                           density,
                                           seed,
                                           k_smoothing,
                                           exp_sigma,
                                           angle_rad,
                                           span_rad,
                                           rtype,
                                           pa_dx,
                                           pa_dy,
                                           region.bbox);
          },
          node.cfg().cm_gpu);
    }

    p_out->remap(0.f, 1.f, node.cfg().cm_cpu);

    if (node.val<bool>(A_SQRT_OUTPUT))
    {
      hmap::for_each_tile(
          {p_out},
          [](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
          {
            hmap::Array *pa_out = p_arrays[0];
            *pa_out             = hmap::sqrt(*pa_out);
          },
          node.cfg().cm_cpu);
    }
  }
  else if (current_group == G_SCATTERED)
  {
    const auto rtype = static_cast<hmap::VoronoiReturnType>(node.val<int>(A_RETURN_TYPE));
    const auto density     = node.val<float>(A_DENSITY);
    const auto variability = node.val<float>(A_VARIABILITY);
    const auto seed        = node.val<int>(A_SEED);
    const auto k_smoothing = node.val<float>(A_K_SMOOTHING);
    const auto exp_sigma   = node.val<float>(A_EXP_SIGMA);

    hmap::for_each_tile(
        {p_out, p_dx, p_dy},
        [&](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &region)
        {
          auto [pa_out, pa_dx, pa_dy] = unpack<3>(p_arrays);
          if (p_cloud && p_cloud->size() > 0)
          {
            *pa_out = hmap::gpu::vororand(region.shape,
                                          p_cloud->get_x(),
                                          p_cloud->get_y(),
                                          k_smoothing,
                                          exp_sigma,
                                          rtype,
                                          pa_dx,
                                          pa_dy,
                                          region.bbox);
          }
          else
          {
            *pa_out = hmap::gpu::vororand(region.shape,
                                          density,
                                          variability,
                                          seed,
                                          k_smoothing,
                                          exp_sigma,
                                          rtype,
                                          pa_dx,
                                          pa_dy,
                                          region.bbox);
          }
        },
        node.cfg().cm_gpu);

    p_out->remap(0.f, 1.f, node.cfg().cm_cpu);

    if (node.val<bool>(A_SQRT_OUTPUT))
    {
      hmap::for_each_tile(
          {p_out},
          [](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
          {
            hmap::Array *pa_out = p_arrays[0];
            *pa_out             = hmap::sqrt(*pa_out);
          },
          node.cfg().cm_cpu);
    }
  }
  else if (current_group == G_VORONOISE)
  {
    const auto kw   = node.val<glm::vec2>(A_KW);
    const auto u    = node.val<float>(A_U);
    const auto v    = node.val<float>(A_V);
    const auto seed = node.val<int>(A_SEED);

    hmap::for_each_tile(
        {p_out, p_dx, p_dy},
        [&](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &region)
        {
          auto [pa_out, pa_dx, pa_dy] = unpack<3>(p_arrays);
          *pa_out                     = hmap::gpu::voronoise(region.shape,
                                         kw,
                                         u,
                                         v,
                                         seed,
                                         pa_dx,
                                         pa_dy,
                                         region.bbox);
        },
        node.cfg().cm_gpu);
  }

  // --- Common Post-Processing
  post_apply_enveloppe(node, *p_out, p_env);
  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
