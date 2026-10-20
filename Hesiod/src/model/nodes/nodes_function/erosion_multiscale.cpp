/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <cmath>

#include "meta/metadata/keys.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/landscape/erosion.hpp"
#include "hesiod/model/nodes/landscape/field_hmap.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_INPUT         = "input";
constexpr const char *P_PRECIPITATION = "precipitation";
constexpr const char *P_MASK          = "mask";
constexpr const char *P_OUTPUT        = "output";
constexpr const char *P_FLOW          = "flow";
constexpr const char *P_WEAR          = "wear";
constexpr const char *P_DEPOSITS      = "deposits";

constexpr const char *A_DURATION           = "duration";
constexpr const char *A_DOWNCUTTING        = "downcutting";
constexpr const char *A_EROSION_SCALE      = "erosion_scale";
constexpr const char *A_SEED               = "seed";
constexpr const char *A_SUSPENDED_AMOUNT   = "suspended_amount";
constexpr const char *A_SUSPENDED_ANGLE    = "suspended_angle";
constexpr const char *A_BED_LOAD_AMOUNT    = "bed_load_amount";
constexpr const char *A_BED_LOAD_ANGLE     = "bed_load_angle";
constexpr const char *A_COARSE_AMOUNT      = "coarse_amount";
constexpr const char *A_COARSE_ANGLE       = "coarse_angle";
constexpr const char *A_SHAPE              = "shape";
constexpr const char *A_SHAPE_SHARPNESS    = "shape_sharpness";
constexpr const char *A_SHAPE_DETAIL_SCALE = "shape_detail_scale";
constexpr const char *A_OROGRAPHIC_ENABLE  = "orographic_enable";
constexpr const char *A_DIRECTIONAL        = "directional_precipitation";
constexpr const char *A_DIRECTION          = "direction";
constexpr const char *A_RAIN_SHADOW        = "rain_shadow";
constexpr const char *A_SLOPE_RANGE        = "slope_range";
constexpr const char *A_ALTITUDE_RANGE     = "altitude_range";
constexpr const char *A_OROGRAPHIC_REVERSE = "orographic_reverse";
constexpr const char *A_WORLD_WIDTH        = "world_width";
constexpr const char *A_WORLD_HEIGHT       = "world_height";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_erosion_multiscale_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_INPUT);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_PRECIPITATION);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUTPUT, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_FLOW, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_WEAR, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_DEPOSITS, CONFIG(node));

  // attribute(s)
  // clang-format off
  node.set_current_category("General");
  add_float(node, A_DURATION, "Duration", 10.f, 1.f, 200.f, "{:.1f}", true);
  add_float(node, A_DOWNCUTTING, "Downcutting", 0.25f, 0.f, 1.f);
  add_float(node, A_EROSION_SCALE, "Erosion Scale", 1000.f, 1.f, 20000.f, "{:.0f} m", true);
  add_seed(node, A_SEED, "Seed");

  node.set_current_category("Sediment Discharge");
  add_float(node, A_SUSPENDED_AMOUNT, "Suspended Load", 1.f, 0.f, 1.f);
  add_float(node, A_SUSPENDED_ANGLE, "Suspended Discharge Angle", 24.f, 0.f, 45.f, "{:.1f}°");
  add_float(node, A_BED_LOAD_AMOUNT, "Bed Load", 0.f, 0.f, 1.f);
  add_float(node, A_BED_LOAD_ANGLE, "Bed Load Discharge Angle", 15.f, 0.f, 45.f, "{:.1f}°");
  add_float(node, A_COARSE_AMOUNT, "Coarse Sediments", 0.f, 0.f, 1.f);
  add_float(node, A_COARSE_ANGLE, "Coarse Discharge Angle", 10.f, 0.f, 45.f, "{:.1f}°");

  node.set_current_category("Shape");
  add_float(node, A_SHAPE, "Shape", 0.2f, 0.f, 1.f);
  add_float(node, A_SHAPE_SHARPNESS, "Sharpness", 0.6f, 0.f, 1.f);
  add_float(node, A_SHAPE_DETAIL_SCALE, "Detail Scale", 0.25f, 0.f, 1.f);

  node.set_current_category("Orographic Influence");
  add_bool(node, A_OROGRAPHIC_ENABLE, "Enable", false);
  add_bool(node, A_DIRECTIONAL, "Directional Precipitation", false);
  add_angle(node, A_DIRECTION, "Direction", 0.f, 0.f, 360.f, "{:.0f}°");
  add_float(node, A_RAIN_SHADOW, "Rain Shadow", 0.8f, 0.f, 1.f);
  add_range(node, A_SLOPE_RANGE, "Slope", {0.f, 90.f}, 0.f, 90.f, true, "{:.0f}°");
  add_range(node, A_ALTITUDE_RANGE, "Altitude", {0.f, 1.f}, 0.f, 1.f, true, "{:.2f}");
  add_bool(node, A_OROGRAPHIC_REVERSE, "Reverse", false);

  node.set_current_category("Advanced");
  add_float(node, A_WORLD_WIDTH, "Terrain Width", 5000.f, 100.f, 100000.f, "{:.0f} m", true);
  add_float(node, A_WORLD_HEIGHT, "Terrain Height", 2500.f, 10.f, 20000.f, "{:.0f} m", true);
  // clang-format on

  setup_pre_process_mask_attributes(node);
  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

static hmap::Array full_array(const hmap::VirtualArray &va,
                              glm::ivec2                shape,
                              const hmap::ComputeMode  &cm)
{
  hmap::Array a = va.to_array(cm);
  if (a.shape != shape)
    a = a.resample_to_shape_bilinear(shape);
  return a;
}

void compute_erosion_multiscale_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  const auto *p_in            = node.get_value_ref<hmap::VirtualArray>(P_INPUT);
  const auto *p_precipitation = node.get_value_ref<hmap::VirtualArray>(P_PRECIPITATION);
  const auto *p_mask          = node.get_value_ref<hmap::VirtualArray>(P_MASK);
  auto       *p_out           = node.get_value_ref<hmap::VirtualArray>(P_OUTPUT);
  auto       *p_flow          = node.get_value_ref<hmap::VirtualArray>(P_FLOW);
  auto       *p_wear          = node.get_value_ref<hmap::VirtualArray>(P_WEAR);
  auto       *p_deposits      = node.get_value_ref<hmap::VirtualArray>(P_DEPOSITS);

  if (!p_in || !p_out)
    return;

  const hmap::ComputeMode &cm    = node.cfg().cm_cpu;
  const glm::ivec2         shape = p_out->shape;

  hmap::Array in = full_array(*p_in, shape, cm);

  // --- parameters

  landscape::ErosionParams ep;
  ep.duration           = node.val<float>(A_DURATION);
  ep.downcutting        = node.val<float>(A_DOWNCUTTING);
  ep.erosion_scale      = node.val<float>(A_EROSION_SCALE);
  ep.seed               = node.val<int>(A_SEED);
  ep.suspended_amount   = node.val<float>(A_SUSPENDED_AMOUNT);
  ep.suspended_angle    = node.val<float>(A_SUSPENDED_ANGLE);
  ep.bed_load_amount    = node.val<float>(A_BED_LOAD_AMOUNT);
  ep.bed_load_angle     = node.val<float>(A_BED_LOAD_ANGLE);
  ep.coarse_amount      = node.val<float>(A_COARSE_AMOUNT);
  ep.coarse_angle       = node.val<float>(A_COARSE_ANGLE);
  ep.shape              = node.val<float>(A_SHAPE);
  ep.shape_sharpness    = node.val<float>(A_SHAPE_SHARPNESS);
  ep.shape_detail_scale = node.val<float>(A_SHAPE_DETAIL_SCALE);
  ep.world_width        = node.val<float>(A_WORLD_WIDTH);
  ep.world_height       = node.val<float>(A_WORLD_HEIGHT);

  // --- the solver is gradient-driven, a flat input is returned unchanged

  auto [hmin, hmax] = std::minmax_element(in.vector.begin(), in.vector.end());
  if (*hmax - *hmin < 1e-6f)
  {
    Logger::log()->warn("ErosionMultiscale [{}]: input is flat, nothing to erode",
                        node.get_id());
    p_out->from_array(in, cm);
    for (auto *p : {p_flow, p_wear, p_deposits})
      if (p)
        p->fill(0.f, cm);
    return;
  }

  // --- erosion on the power-of-two working grid

  const int        n      = landscape::working_size(shape);
  landscape::Field height = landscape::to_field(in, n);

  // spawn mask: orographic mask times the precipitation input
  landscape::Field spawn;

  if (node.val<bool>(A_OROGRAPHIC_ENABLE))
  {
    landscape::PrecipitationParams pp;
    pp.world_width  = ep.world_width;
    pp.world_height = ep.world_height;
    pp.directional  = node.val<bool>(A_DIRECTIONAL);
    pp.direction    = node.val<float>(A_DIRECTION);
    pp.rain_shadow  = node.val<float>(A_RAIN_SHADOW);
    pp.reverse      = node.val<bool>(A_OROGRAPHIC_REVERSE);

    if (node.state_val<bool>(A_SLOPE_RANGE, meta::keys::state::active))
    {
      glm::vec2 r  = node.val<glm::vec2>(A_SLOPE_RANGE);
      pp.slope_min = r.x;
      pp.slope_max = r.y;
    }
    if (node.state_val<bool>(A_ALTITUDE_RANGE, meta::keys::state::active))
    {
      glm::vec2 r     = node.val<glm::vec2>(A_ALTITUDE_RANGE);
      pp.altitude_min = r.x;
      pp.altitude_max = r.y;
    }

    spawn = landscape::precipitation_mask(height, pp);
  }

  if (p_precipitation)
  {
    landscape::Field ext = landscape::to_field(full_array(*p_precipitation, shape, cm),
                                               n);
    for (float &v : ext.data)
      v = std::clamp(v, 0.f, 1.f);

    if (spawn.n == n)
      for (size_t i = 0; i < spawn.data.size(); i++)
        spawn.data[i] *= ext.data[i];
    else
      spawn = std::move(ext);
  }

  landscape::ErosionResult res = landscape::erode(std::move(height),
                                                  ep,
                                                  spawn.n == n ? &spawn : nullptr);

  // --- terrain, blended with the mask (connected or default definition)

  hmap::Array out = landscape::to_array(std::move(res.height), shape);

  hmap::VirtualArray mask_default = pre_process_mask(node, p_mask, *p_in);
  if (!mask_default.empty())
    p_mask = &mask_default;

  if (p_mask)
  {
    hmap::Array mask = full_array(*p_mask, shape, cm);
    for (size_t i = 0; i < out.vector.size(); i++)
      out.vector[i] = in.vector[i] + (out.vector[i] - in.vector[i]) * mask.vector[i];
  }

  p_out->from_array(out, cm);

  // --- auxiliary outputs, normalized to [0, 1]

  if (p_flow)
  {
    for (float &v : res.flow.data)
      v = std::log1p(v);
    p_flow->from_array(landscape::to_array(std::move(res.flow), shape), cm);
    p_flow->remap(0.f, 1.f, cm);
  }

  if (p_wear)
  {
    p_wear->from_array(landscape::to_array(std::move(res.wear), shape), cm);
    p_wear->remap(0.f, 1.f, cm);
  }

  if (p_deposits)
  {
    p_deposits->from_array(landscape::to_array(std::move(res.deposits), shape), cm);
    p_deposits->remap(0.f, 1.f, cm);
  }

  // --- post-process

  post_process_heightmap(node, *p_out, p_in);
}

} // namespace hesiod
