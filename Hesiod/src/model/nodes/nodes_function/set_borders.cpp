/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/boundary.hpp"

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
constexpr const char *P_OUT = "output";

constexpr const char *A_RADIUS         = "radius";
constexpr const char *A_UNIFORM_RADIUS = "uniform_radius";
constexpr const char *A_RADIUS_WEST    = "radius_west";
constexpr const char *A_RADIUS_EAST    = "radius_east";
constexpr const char *A_RADIUS_NORTH   = "radius_north";
constexpr const char *A_RADIUS_SOUTH   = "radius_south";
constexpr const char *A_UNIFORM_VALUE  = "uniform_value";
constexpr const char *A_VALUE_WEST     = "value_west";
constexpr const char *A_VALUE_EAST     = "value_east";
constexpr const char *A_VALUE_NORTH    = "value_north";
constexpr const char *A_VALUE_SOUTH    = "value_south";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_set_borders_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Attributes

  // clang-format off
  add_float(node, A_RADIUS, "Radius", 0.4f, 0.f, 0.5f);
  add_bool(node, A_UNIFORM_RADIUS, "Uniform Radius", true);
  add_float(node, A_RADIUS_WEST, "West", 0.4f, 0.f, 0.5f);
  add_float(node, A_RADIUS_EAST, "East", 0.4f, 0.f, 0.5f);
  add_float(node, A_RADIUS_NORTH, "North", 0.4f, 0.f, 0.5f);
  add_float(node, A_RADIUS_SOUTH, "South", 0.4f, 0.f, 0.5f);
  add_bool(node, A_UNIFORM_VALUE, "Uniform Value", false);
  add_float(node, A_VALUE_WEST, "West", -0.5f, -FLT_MAX, FLT_MAX);
  add_float(node, A_VALUE_EAST, "East", 0.5f, -FLT_MAX, FLT_MAX);
  add_float(node, A_VALUE_NORTH, "North", 0.5f, -FLT_MAX, FLT_MAX);
  add_float(node, A_VALUE_SOUTH, "South", -0.5f, -FLT_MAX, FLT_MAX);
  // clang-format on

  // --- Attribute order
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_set_borders_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_in  = node.get_value_ref<hmap::VirtualArray>(P_IN);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_in || !p_out)
    return;

  // --- Helpers

  const auto to_px_x = [&](float r) { return std::max(1, int(r * p_in->shape.x)); };

  const auto to_px_y = [&](float r) { return std::max(1, int(r * p_in->shape.y)); };

  // --- Border radii

  // engine order: {west=.x, east=.y, south=.z, north=.w}
  glm::ivec4 buffer_sizes;

  if (node.val<bool>(A_UNIFORM_RADIUS))
  {
    const float r = node.val<float>(A_RADIUS);

    buffer_sizes = {
        to_px_x(r), // west
        to_px_x(r), // east
        to_px_y(r), // south
        to_px_y(r)  // north
    };
  }
  else
  {
    buffer_sizes = {to_px_x(node.val<float>(A_RADIUS_WEST)),
                    to_px_x(node.val<float>(A_RADIUS_EAST)),
                    to_px_y(node.val<float>(A_RADIUS_SOUTH)),
                    to_px_y(node.val<float>(A_RADIUS_NORTH))};
  }

  // --- Border values

  const float vw = node.val<float>(A_VALUE_WEST);
  const float ve = node.val<float>(A_VALUE_EAST);
  const float vn = node.val<float>(A_VALUE_NORTH);
  const float vs = node.val<float>(A_VALUE_SOUTH);

  glm::vec4 border_values;

  if (node.val<bool>(A_UNIFORM_VALUE))
  {
    border_values = {vw, vw, vw, vw};
  }
  else
  {
    // engine order: {west, east, south, north}
    border_values = {vw, ve, vs, vn};
  }

  // --- Compute

  hmap::for_each_tile(
      {p_out, p_in},
      [buffer_sizes, border_values](std::vector<hmap::Array *> p_arrays,
                                    const hmap::TileRegion &)
      {
        auto [pa_out, pa_in] = unpack<2>(p_arrays);

        *pa_out = *pa_in;

        hmap::set_borders(*pa_out, border_values, buffer_sizes);
      },
      node.cfg().cm_single_array);
}

} // namespace hesiod
