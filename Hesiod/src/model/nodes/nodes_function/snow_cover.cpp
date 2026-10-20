/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>

#include "highmap/virtual_array/virtual_texture.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/landscape/field_hmap.hpp"
#include "hesiod/model/nodes/landscape/snow.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_INPUT      = "input";
constexpr const char *P_SNOWFALL   = "snowfall";
constexpr const char *P_MELT       = "melt";
constexpr const char *P_TEXTURE_IN = "texture_in";
constexpr const char *P_MASK       = "mask";
constexpr const char *P_OUTPUT     = "output";
constexpr const char *P_SNOW       = "snow";
constexpr const char *P_HARD       = "hard";
constexpr const char *P_DEPTH      = "depth";
constexpr const char *P_TEXTURE    = "texture";

constexpr const char *A_DURATION       = "duration";
constexpr const char *A_INTENSITY      = "intensity";
constexpr const char *A_SETTLE         = "settle_duration";
constexpr const char *A_MELT_TYPE      = "melt_type";
constexpr const char *A_MELT           = "melt";
constexpr const char *A_MELT_REMNANTS  = "melt_remnants";
constexpr const char *A_DIRECTION      = "direction";
constexpr const char *A_SNOW_LINE      = "snow_line";
constexpr const char *A_SLIP_OFF_ANGLE = "slip_off_angle";
constexpr const char *A_ADHERED_MASS   = "adhered_snow_mass";
constexpr const char *A_MODEL_SCALE    = "model_scale";
constexpr const char *A_REAL_SCALE     = "real_scale";
constexpr const char *A_WORLD_WIDTH    = "world_width";
constexpr const char *A_WORLD_HEIGHT   = "world_height";
constexpr const char *A_TERRAIN_SCALE  = "terrain_scale";
constexpr const char *A_VERTICALITY    = "verticality";
constexpr const char *A_SNOW_COLOR     = "snow_color";
constexpr const char *A_GROUND_COLOR   = "ground_color";

namespace
{
enum MeltType : int
{
  MELT_UNIFORM     = 0,
  MELT_DIRECTIONAL = 1,
};
} // namespace

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_snow_cover_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_INPUT);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_SNOWFALL);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MELT);
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, P_TEXTURE_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUTPUT, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_SNOW, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_HARD, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_DEPTH, CONFIG(node));
  node.add_port<hmap::VirtualTexture>(gnode::PortType::OUT, P_TEXTURE, CONFIG_TEX(node));

  // attribute(s), the enum values are serialized: append only, never renumber
  // clang-format off
  node.set_current_category("Snow");
  add_float(node, A_DURATION, "Duration", 0.25f, 0.f, 1.f);
  add_float(node, A_INTENSITY, "Intensity", 0.25f, 0.f, 1.f);
  add_float(node, A_SETTLE, "Settle Duration", 0.25f, 0.f, 1.f);

  node.set_current_category("Melt");
  add_enum(node, A_MELT_TYPE, "Melt Type", {{MELT_UNIFORM, "Uniform"}, {MELT_DIRECTIONAL, "Directional"}}, MELT_UNIFORM);
  add_float(node, A_MELT, "Melt", 0.f, 0.f, 1.f);
  add_float(node, A_MELT_REMNANTS, "Melt Remnants", 0.f, 0.f, 1.f);
  add_angle(node, A_DIRECTION, "Direction", 0.f, 0.f, 360.f, "{:.0f}°");
  add_float(node, A_SNOW_LINE, "Snow Line", 0.f, 0.f, 1.f);

  node.set_current_category("Snow Layer");
  add_color(node, A_SNOW_COLOR, "Snow Color", {0.94f, 0.95f, 0.97f, 1.f});
  add_color(node, A_GROUND_COLOR, "Ground Color", {0.6f, 0.6f, 0.6f, 1.f});

  node.set_current_category("Advanced");
  add_float(node, A_SLIP_OFF_ANGLE, "Slip-off Angle", 38.f, 0.f, 90.f, "{:.1f}°");
  add_float(node, A_ADHERED_MASS, "Adhered Snow Mass", 12.f, 0.f, 30.f);
  add_float(node, A_MODEL_SCALE, "Model Scale", 5.f, 1.f, 16.f);
  add_bool(node, A_REAL_SCALE, "Real Scale", true);
  add_float(node, A_WORLD_WIDTH, "Terrain Width", 5000.f, 100.f, 100000.f, "{:.0f} m", true);
  add_float(node, A_WORLD_HEIGHT, "Terrain Height", 2500.f, 10.f, 20000.f, "{:.0f} m", true);
  add_int(node, A_TERRAIN_SCALE, "Terrain Scale", 5000, 500, 20000);
  add_int(node, A_VERTICALITY, "Verticality", 3200, 50, 20000);
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

static landscape::Field clamped_field(const hmap::VirtualArray &va,
                                      glm::ivec2                shape,
                                      int                       n,
                                      const hmap::ComputeMode  &cm)
{
  landscape::Field f = landscape::to_field(full_array(va, shape, cm), n);
  for (float &v : f.data)
    v = std::clamp(v, 0.f, 1.f);
  return f;
}

void compute_snow_cover_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  const auto *p_in       = node.get_value_ref<hmap::VirtualArray>(P_INPUT);
  const auto *p_snowfall = node.get_value_ref<hmap::VirtualArray>(P_SNOWFALL);
  const auto *p_melt     = node.get_value_ref<hmap::VirtualArray>(P_MELT);
  const auto *p_tex_in   = node.get_value_ref<hmap::VirtualTexture>(P_TEXTURE_IN);
  const auto *p_mask     = node.get_value_ref<hmap::VirtualArray>(P_MASK);
  auto       *p_out      = node.get_value_ref<hmap::VirtualArray>(P_OUTPUT);
  auto       *p_snow     = node.get_value_ref<hmap::VirtualArray>(P_SNOW);
  auto       *p_hard     = node.get_value_ref<hmap::VirtualArray>(P_HARD);
  auto       *p_depth    = node.get_value_ref<hmap::VirtualArray>(P_DEPTH);
  auto       *p_tex      = node.get_value_ref<hmap::VirtualTexture>(P_TEXTURE);

  if (!p_in || !p_out)
    return;

  const hmap::ComputeMode &cm    = node.cfg().cm_cpu;
  const glm::ivec2         shape = p_out->shape;

  // --- parameters

  const bool real_scale = node.val<bool>(A_REAL_SCALE);

  landscape::SnowParams sp;
  sp.duration          = node.val<float>(A_DURATION);
  sp.intensity         = node.val<float>(A_INTENSITY);
  sp.settle            = node.val<float>(A_SETTLE);
  sp.melt              = node.val<float>(A_MELT);
  sp.snow_line         = node.val<float>(A_SNOW_LINE);
  sp.slip_off_angle    = node.val<float>(A_SLIP_OFF_ANGLE);
  sp.adhered_snow_mass = node.val<float>(A_ADHERED_MASS);
  sp.model_scale       = node.val<float>(A_MODEL_SCALE);
  sp.terrain_width     = real_scale ? node.val<float>(A_WORLD_WIDTH)
                                    : float(node.val<int>(A_TERRAIN_SCALE));
  sp.terrain_height    = real_scale ? node.val<float>(A_WORLD_HEIGHT)
                                    : float(node.val<int>(A_VERTICALITY));

  // --- simulation on the power-of-two working grid

  hmap::Array      in     = full_array(*p_in, shape, cm);
  const int        n      = landscape::working_size(shape);
  landscape::Field height = landscape::to_field(in, n);

  landscape::Field snowfall, melt;
  if (p_snowfall)
    snowfall = clamped_field(*p_snowfall, shape, n, cm);

  // a connected melt map wins, otherwise the directional melt map is derived
  // from the input terrain
  if (p_melt)
    melt = clamped_field(*p_melt, shape, n, cm);
  else if (node.val<int>(A_MELT_TYPE) == MELT_DIRECTIONAL)
    melt = landscape::directional_melt_map(height,
                                           node.val<float>(A_DIRECTION),
                                           node.val<float>(A_MELT_REMNANTS),
                                           sp.terrain_width,
                                           sp.terrain_height);

  landscape::SnowResult res = landscape::snow(height,
                                              sp,
                                              snowfall.n == n ? &snowfall : nullptr,
                                              melt.n == n ? &melt : nullptr);

  // --- terrain with snow, blended with the mask (connected or default)

  hmap::Array out = landscape::to_array(std::move(res.height), shape);

  hmap::VirtualArray mask_default = pre_process_mask(node, p_mask, *p_in);
  if (!mask_default.empty())
    p_mask = &mask_default;

  hmap::Array mask;
  if (p_mask)
  {
    mask = full_array(*p_mask, shape, cm);
    for (size_t i = 0; i < out.vector.size(); i++)
      out.vector[i] = in.vector[i] + (out.vector[i] - in.vector[i]) * mask.vector[i];
  }

  p_out->from_array(out, cm);

  // --- snow layers

  hmap::Array snow = landscape::to_array(std::move(res.snow), shape);
  if (p_mask)
    for (size_t i = 0; i < snow.vector.size(); i++)
      snow.vector[i] *= mask.vector[i];

  if (p_snow)
    p_snow->from_array(snow, cm);
  if (p_hard)
    p_hard->from_array(landscape::to_array(std::move(res.hard), shape), cm);
  if (p_depth)
    p_depth->from_array(landscape::to_array(std::move(res.depth), shape), cm);

  // viewer layer: snow over the incoming texture, or over a flat ground color
  if (p_tex)
  {
    const glm::vec4 snow_color   = node.val<glm::vec4>(A_SNOW_COLOR);
    const glm::vec4 ground_color = node.val<glm::vec4>(A_GROUND_COLOR);

    for (int ch = 0; ch < 4; ch++)
    {
      hmap::Array base(shape, ground_color[ch]);
      if (p_tex_in && ch < p_tex_in->channels())
        base = full_array(p_tex_in->channel(ch), shape, cm);

      for (size_t i = 0; i < base.vector.size(); i++)
        base.vector[i] += (snow_color[ch] - base.vector[i]) * snow.vector[i];

      p_tex->channel(ch).from_array(base, cm);
    }
  }

  // --- post-process

  post_process_heightmap(node, *p_out, p_in);
}

} // namespace hesiod
