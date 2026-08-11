/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/hydrology/hydrology.hpp"
#include "highmap/range.hpp"

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

constexpr const char *P_ELEVATION_IN   = "elevation_in";
constexpr const char *P_DEPTH_MAP      = "depth_map";
constexpr const char *P_SNOW_DEPTH_IN  = "snow_depth_in";
constexpr const char *P_MELTING_MAP    = "melting_map";
constexpr const char *P_ELEVATION_OUT  = "elevation";
constexpr const char *P_SNOW_DEPTH_OUT = "snow_depth";

constexpr const char *A_SNOW_DEPTH          = "snow_depth";
constexpr const char *A_TALUS_GLOBAL        = "talus_global";
constexpr const char *A_DURATION            = "duration";
constexpr const char *A_SOLVER_STRIDE       = "solver_stride";
constexpr const char *A_DMAP_TYPE           = "depth_map_type";
constexpr const char *A_POST_FILTER         = "post_filter";
constexpr const char *A_THERMAL_TALUS_RATIO = "thermal_talus_ratio";
constexpr const char *A_K_SNOW              = "k_snow";
constexpr const char *A_K_VISC              = "k_visc";
constexpr const char *A_K_MELT_FACTOR       = "k_melt_factor";
constexpr const char *A_K_DEPTH_RATIO       = "k_depth_ratio";
constexpr const char *A_K_DEPTH_SLOPE_RATIO = "k_depth_slope_ratio";
constexpr const char *A_SHIFT_TO_ZERO       = "shift_to_zero";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_snow_simulation_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ELEVATION_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DEPTH_MAP);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_SNOW_DEPTH_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MELTING_MAP);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_ELEVATION_OUT, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_SNOW_DEPTH_OUT, CONFIG(node));

  // attribute(s)
  // clang-format off
  add_float(node, A_SNOW_DEPTH, "Snow Amount", 0.1f, 0.001f, 0.5f, "{:.2e}", /* log */ true);
  add_float(node, A_TALUS_GLOBAL, "Base Repose Slope", 1.5f, 0.f, FLT_MAX);
  add_float(node, A_DURATION, "Simulation Duration", 1.f, 0.f, 2.f);
  add_int(node, A_SOLVER_STRIDE, "Solver Stride", 4, 1, 32);
  add_enum(node, A_DMAP_TYPE, "Predefined Depth Map", DefaultMapOptions::type_map(), "Uniform");
  add_bool(node, A_POST_FILTER, "Enable Thermal Relaxation", true);
  add_float(node, A_THERMAL_TALUS_RATIO, "Thermal Repose Ratio", 0.2f, 0.01f, 1.f);
  add_float(node, A_K_SNOW, "Avalanche Strength", 0.5f, 0.f, 1.f);
  add_float(node, A_K_VISC, "Thermal Creep Strength", 0.01f, 0.f, 0.2f);
  add_float(node, A_K_MELT_FACTOR, "Melting Strength", 0.8f, 0.f, 1.f);
  add_float(node, A_K_DEPTH_RATIO, "Depth Stiffening", 1.f, 0.f, 1.f);
  add_float(node, A_K_DEPTH_SLOPE_RATIO, "Depth Repose Boost", 2.f, 0.f, 4.f);
  add_bool(node, A_SHIFT_TO_ZERO, "Rebase Snow to Zero", true);
  // clang-format on
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_snow_simulation_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  auto *p_z = node.get_value_ref<hmap::VirtualArray>(P_ELEVATION_IN);
  if (!p_z)
    return;

  auto *p_depth_map     = node.get_value_ref<hmap::VirtualArray>(P_DEPTH_MAP);
  auto *p_snow_depth_in = node.get_value_ref<hmap::VirtualArray>(P_SNOW_DEPTH_IN);
  auto *p_snow_depth    = node.get_value_ref<hmap::VirtualArray>(P_SNOW_DEPTH_OUT);
  auto *p_melting_map   = node.get_value_ref<hmap::VirtualArray>(P_MELTING_MAP);
  auto *p_z_out         = node.get_value_ref<hmap::VirtualArray>(P_ELEVATION_OUT);

  // --- Parameters wrapper

  const auto params = [&node, p_z]()
  {
    struct P
    {
      int   solver_stride;
      int   dmap_type;
      float snow_depth;
      float k_snow, k_visc, k_melt_factor;
      float k_depth_ratio, k_depth_slope_ratio;
      float thermal_talus_ratio;
      bool  post_filter;
      bool  shift_to_zero;
      //
      int   nx_strided;
      int   iterations;
      float talus;
    };

    const int   nx         = p_z->shape.x;
    const int   stride     = node.val<int>(A_SOLVER_STRIDE);
    const float duration   = node.val<float>(A_DURATION);
    const int   nx_strided = int(float(nx) / stride);
    const int   iterations = int(duration * nx_strided);
    const float talus      = node.val<float>(A_TALUS_GLOBAL) / nx_strided;

    return P{.solver_stride       = stride,
             .dmap_type           = node.val<int>(A_DMAP_TYPE),
             .snow_depth          = node.val<float>(A_SNOW_DEPTH),
             .k_snow              = node.val<float>(A_K_SNOW),
             .k_visc              = node.val<float>(A_K_VISC),
             .k_melt_factor       = node.val<float>(A_K_MELT_FACTOR),
             .k_depth_ratio       = node.val<float>(A_K_DEPTH_RATIO),
             .k_depth_slope_ratio = node.val<float>(A_K_DEPTH_SLOPE_RATIO),
             .thermal_talus_ratio = node.val<float>(A_THERMAL_TALUS_RATIO),
             .post_filter         = node.val<bool>(A_POST_FILTER),
             .shift_to_zero       = node.val<bool>(A_SHIFT_TO_ZERO),
             //
             .nx_strided = nx_strided,
             .iterations = iterations,
             .talus      = talus};
  }();

  // --- Adjust compute mode (stride)

  // override compute mode (but keep storage mode)
  hmap::ComputeMode cm = node.cfg().cm_gpu;
  cm.stride            = params.solver_stride;

  // --- Create talus virtual array

  hmap::VirtualArray talus_map = hmap::VirtualArray(CONFIG(node));
  talus_map.fill(params.talus, node.cfg().cm_cpu);

  // --- Create melting map if not available

  hmap::VirtualArray melting_map = hmap::VirtualArray(CONFIG(node));

  if (!p_melting_map)
  {
    p_melting_map = &melting_map;
    melting_map.fill(0.f, node.cfg().cm_cpu);
  }

  // --- Resolve depth map source

  // may be overriden when tne snow depth is set as an input
  float snow_depth_updated = params.snow_depth;

  hmap::VirtualArray dmap(CONFIG(node));

  // provided input snow depth has priority, if not use the a
  // uniform depth map if the depth map input is not set
  if (p_snow_depth_in)
  {
    copy_data(*p_snow_depth_in, dmap, node.cfg().cm_cpu);
    snow_depth_updated = dmap.max(node.cfg().cm_cpu);
    p_depth_map        = &dmap;
  }
  else if (!p_depth_map)
  {
    auto map_type = DefaultMapOptions::Type(params.dmap_type);
    auto options  = DefaultMapOptions{.map_type = map_type};

    generate_map(node, p_depth_map, dmap, options);
  }

  // --- Snow simulation

  // retrieve depth range for normalization
  float dmin = p_depth_map->min(node.cfg().cm_cpu);
  float dmax = p_depth_map->max(node.cfg().cm_cpu);

  if (dmin == dmax)
  {
    dmin = 0.f;
    dmax = 1.f;
  }

  // eventually compute
  hmap::for_each_tile(
      {p_z, p_depth_map, p_melting_map, &talus_map},
      {p_z_out, p_snow_depth},
      [&node, &params, dmin, dmax, snow_depth_updated](
          std::vector<const hmap::Array *> p_arrays_in,
          std::vector<hmap::Array *>       p_arrays_out,
          const hmap::TileRegion &)
      {
        auto [pa_z, pa_depth_map, pa_melting_map, pa_talus_map] = unpack<4>(p_arrays_in);
        auto [pa_z_out, pa_snow_depth]                          = unpack<2>(p_arrays_out);

        hmap::Array depth_map_scaled = *pa_depth_map;
        hmap::remap(depth_map_scaled, 0.f, 1.f, dmin, dmax);

        *pa_snow_depth = hmap::gpu::snow_simulation(*pa_z,
                                                    snow_depth_updated,
                                                    *pa_depth_map,
                                                    *pa_melting_map,
                                                    *pa_talus_map,
                                                    params.iterations,
                                                    /* dt */ 0.5f,
                                                    /* fall_iterations_ratio */ 1.f,
                                                    params.k_snow,
                                                    params.k_visc,
                                                    params.k_melt_factor,
                                                    params.k_depth_ratio,
                                                    params.k_depth_slope_ratio,
                                                    params.post_filter,
                                                    params.thermal_talus_ratio);

        *pa_z_out = *pa_z + *pa_snow_depth;
      },
      cm);

  // --- clean-up output

  // force minimum snow depth to be actually "zero" (more
  // convenient)
  if (params.shift_to_zero)
    p_snow_depth->remap(0.f, p_snow_depth->max(node.cfg().cm_cpu), node.cfg().cm_cpu);
}

} // namespace hesiod
