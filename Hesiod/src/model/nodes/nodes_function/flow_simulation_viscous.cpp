/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/hydrology/hydrology.hpp"
#include "highmap/primitives.hpp"
#include "highmap/range.hpp"

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

constexpr const char *P_ELEVATION = "elevation_in";
constexpr const char *P_DEPTH_MAP = "depth_map";
constexpr const char *P_DEPTH_IN = "depth_in";
constexpr const char *P_ELEVATION_OUT = "elevation";
constexpr const char *P_DEPTH_OUT = "depth";

constexpr const char *A_INITIAL_DEPTH = "initial_depth";
constexpr const char *A_DURATION = "simulation_duration";
constexpr const char *A_POWER = "flow_power";
constexpr const char *A_SOLVER_STRIDE = "solver_stride";
constexpr const char *A_DMAP_TYPE = "depth_map_type";
constexpr const char *A_POST_FILTER = "post_filter";
constexpr const char *A_FILTER_RADIUS = "filter_radius";
constexpr const char *A_SHIFT_TO_ZERO = "shift_to_zero";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_flow_simulation_viscous_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ELEVATION);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DEPTH_MAP);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DEPTH_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_ELEVATION_OUT, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_DEPTH_OUT, CONFIG(node));

  // attribute(s)
  // clang-format off
  node.add_attr<FloatAttribute>(A_INITIAL_DEPTH, "Initial Material Depth", 0.1f, 0.01f, 0.5f, "{:.2e}", /* log */ true);
  node.add_attr<FloatAttribute>(A_DURATION, "Simulation Duration", 1.f, 0.f, FLT_MAX);
  node.add_attr<FloatAttribute>(A_POWER, "Flow Power", 2.5f, 1.f, 4.f);
  node.add_attr<IntAttribute>(A_SOLVER_STRIDE, "Solver Iteration Stride", 8, 1, 32);
  node.add_attr<EnumAttribute>(A_DMAP_TYPE, "Predefined Depth Map", DefaultMapOptions::type_map());
  node.add_attr<BoolAttribute>(A_POST_FILTER, "Enable Post Filtering", true);
  node.add_attr<FloatAttribute>(A_FILTER_RADIUS, "Filter Radius", 0.05f, 0.f, 0.2f);
  node.add_attr<BoolAttribute>(A_SHIFT_TO_ZERO, "Rebase Depth to Zero", true);
  // clang-format on

  // attribute(s) order
  node.set_attr_ordered_key({"_GROUPBOX_BEGIN_Material Setup",
                             A_INITIAL_DEPTH,
                             A_DMAP_TYPE,
                             A_DURATION,
                             A_POWER,
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Post-filter",
                             A_POST_FILTER,
                             A_FILTER_RADIUS,
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Output Conditioning",
                             A_SHIFT_TO_ZERO,
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Solver",
                             A_SOLVER_STRIDE,
                             "_GROUPBOX_END_"});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_flow_simulation_viscous_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  auto *p_z = node.get_value_ref<hmap::VirtualArray>(P_ELEVATION);

  if (!p_z)
    return;

  auto *p_depth_map = node.get_value_ref<hmap::VirtualArray>(P_DEPTH_MAP);
  auto *p_depth_in = node.get_value_ref<hmap::VirtualArray>(P_DEPTH_IN);
  auto *p_z_out = node.get_value_ref<hmap::VirtualArray>(P_ELEVATION_OUT);
  auto *p_depth_out = node.get_value_ref<hmap::VirtualArray>(P_DEPTH_OUT);

  // --- Parameters wrapper

  const auto params = [&node, p_depth_out]()
  {
    struct P
    {
      float initial_depth;
      float power;
      int   solver_stride;
      int   dmap_type;
      bool  post_filter;
      bool  shift_to_zero;
      //
      int nx_strided;
      int iterations;
      int ir;
    };

    const int   nx = p_depth_out->shape.x;
    const int   stride = node.get_attr<IntAttribute>(A_SOLVER_STRIDE);
    const float radius = node.get_attr<FloatAttribute>(A_FILTER_RADIUS);
    const float duration = node.get_attr<FloatAttribute>(A_DURATION);
    const int   nx_strided = int(float(nx) / stride);
    const int   iterations = int(duration * nx_strided);
    const int   ir = std::max(1, int(radius * nx));

    return P{.initial_depth = node.get_attr<FloatAttribute>(A_INITIAL_DEPTH),
             .power = node.get_attr<FloatAttribute>(A_POWER),
             .solver_stride = stride,
             .dmap_type = node.get_attr<EnumAttribute>(A_DMAP_TYPE),
             .post_filter = node.get_attr<BoolAttribute>(A_POST_FILTER),
             .shift_to_zero = node.get_attr<BoolAttribute>(A_SHIFT_TO_ZERO),
             //
             .nx_strided = nx_strided,
             .iterations = iterations,
             .ir = ir};
  }();

  // --- Adjust compute mode (stride)

  // override compute mode (but keep storage mode)
  hmap::ComputeMode cm = node.cfg().cm_gpu;
  cm.stride = params.solver_stride;

  // --- Resolve depth map source

  // may be overriden when tne snow depth is set as an input
  float mat_depth_updated = params.initial_depth;

  hmap::VirtualArray dmap(CONFIG(node));

  if (p_depth_in)
  {
    copy_data(*p_depth_in, dmap, node.cfg().cm_cpu);
    mat_depth_updated = dmap.max(node.cfg().cm_cpu);
    p_depth_map = &dmap;
  }
  else if (!p_depth_map)
  {
    auto map_type = DefaultMapOptions::Type(params.dmap_type);
    auto options = DefaultMapOptions{.map_type = map_type};

    generate_map(node, p_depth_map, dmap, options);
  }

  // --- Simulation

  // retrieve depth range for normalization
  float dmin = p_depth_map->min(node.cfg().cm_cpu);
  float dmax = p_depth_map->max(node.cfg().cm_cpu);

  if (dmin == dmax)
  {
    dmin = 0.f;
    dmax = 1.f;
  }

  // compute
  hmap::for_each_tile(
      {p_z, p_depth_map},
      {p_z_out, p_depth_out},
      [&node, &params, dmin, dmax, mat_depth_updated](
          std::vector<const hmap::Array *> p_arrays_in,
          std::vector<hmap::Array *>       p_arrays_out,
          const hmap::TileRegion &)
      {
        auto [pa_z, pa_depth_map] = unpack<2>(p_arrays_in);
        auto [pa_z_out, pa_depth_out] = unpack<2>(p_arrays_out);

        hmap::Array depth_map_scaled = *pa_depth_map;
        hmap::remap(depth_map_scaled, 0.f, 1.f, dmin, dmax);

        *pa_depth_out = hmap::gpu::flow_simulation_viscous(*pa_z,
                                                           mat_depth_updated,
                                                           depth_map_scaled,
                                                           params.iterations,
                                                           /* dt */ 1e-5f,
                                                           /* dry_out_ratio */ 0.f,
                                                           /* viscosity */ 1.f,
                                                           params.power);

        *pa_z_out = *pa_z + *pa_depth_out;
      },
      cm);

  // --- clean-up output

  // post-filter
  if (params.post_filter)
  {

    hmap::for_each_tile(
        {p_z, p_depth_out},
        [&node, &params](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_z, pa_depth_out] = unpack<2>(p_arrays);
          hmap::gpu::water_depth_filter(*pa_depth_out, *pa_z, params.ir);
        },
        node.cfg().cm_gpu);
  }

  // force minimum snow depth to be actually "zero" (more
  // convenient)
  if (params.shift_to_zero)
    p_depth_out->remap(0.f, p_depth_out->max(node.cfg().cm_cpu), node.cfg().cm_cpu);
}

} // namespace hesiod
