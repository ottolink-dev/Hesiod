/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/primitives.hpp"

#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_DX = "dx";
constexpr const char *P_DY = "dy";
constexpr const char *P_CONTROL = "control";
constexpr const char *P_ENVELOPE = "envelope";
constexpr const char *P_OUT = "output";

constexpr const char *A_ANGLE = "angle";
constexpr const char *A_R = "r";
constexpr const char *A_NSTEPS = "nsteps";
constexpr const char *A_SCALE = "scale";
constexpr const char *A_CENTER = "center";
constexpr const char *A_OUTER_SLOPE = "outer_slope";
constexpr const char *A_ELEVATION_EXP = "elevation_exponent";
constexpr const char *A_SHAPE_GAIN = "shape_gain";
constexpr const char *A_SEED = "seed";
constexpr const char *A_KW = "kw";
constexpr const char *A_NOISE_AMP = "noise_amp";
constexpr const char *A_NOISE_RUGOSITY = "noise_rugosity";
constexpr const char *A_NOISE_INFLATE = "noise_inflate";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_multisteps_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DX);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DY);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_CONTROL);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ENVELOPE);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Attributes

  // clang-format off
  node.add_attr<FloatAttribute>(A_ANGLE, "Rotation Angle", 0.f, -180.f, 180.f, "{:.1f}°");
  node.add_attr<FloatAttribute>(A_R, "Step Width Ratio", 1.4f, 0.01f, 3.f);
  node.add_attr<IntAttribute>(A_NSTEPS, "Number of Steps", 5, 1, 32);
  node.add_attr<FloatAttribute>(A_SCALE, "Overall Scale", 0.5f, 0.01f, 2.f);
  node.add_attr<Vec2FloatAttribute>(A_CENTER, "Center");
  node.add_attr<FloatAttribute>(A_OUTER_SLOPE, "Outer Slope", 0.05f, 0.f, 1.f);
  node.add_attr<FloatAttribute>(A_ELEVATION_EXP, "Elevation Exponent", 0.7f, 0.01f, 2.f);
  node.add_attr<FloatAttribute>(A_SHAPE_GAIN, "Transition Shape Gain", 4.f, 0.01f, 8.f);
  node.add_attr<SeedAttribute>(A_SEED, "Seed");
  node.add_attr<WaveNbAttribute>(A_KW, "Spatial Frequency");
  node.add_attr<FloatAttribute>(A_NOISE_AMP, "Amplitude", 0.1f, 0.f, 1.f);
  node.add_attr<FloatAttribute>(A_NOISE_RUGOSITY, "Smoothness", 0.f, 0.f, 1.f);
  node.add_attr<BoolAttribute>(A_NOISE_INFLATE, "", "Inflate", "Deflate", true);
  // clang-format on

  // --- Attribute(s) order

  node.set_attr_ordered_key({"_GROUPBOX_BEGIN_Geometry",
                             A_ANGLE,
                             A_R,
                             A_NSTEPS,
                             A_SCALE,
                             A_CENTER,
                             A_OUTER_SLOPE,
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Shape Controls",
                             A_ELEVATION_EXP,
                             A_SHAPE_GAIN,
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Noise Parameters",
                             A_SEED,
                             A_KW,
                             A_NOISE_AMP,
                             A_NOISE_RUGOSITY,
                             A_NOISE_INFLATE,
                             "_GROUPBOX_END_"});

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = true});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_multisteps_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_dx = node.get_value_ref<hmap::VirtualArray>(P_DX);
  auto *p_dy = node.get_value_ref<hmap::VirtualArray>(P_DY);
  auto *p_control = node.get_value_ref<hmap::VirtualArray>(P_CONTROL);
  auto *p_envelope = node.get_value_ref<hmap::VirtualArray>(P_ENVELOPE);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  // --- Params

  // clang-format off
  const auto angle          = node.get_attr<FloatAttribute>(A_ANGLE);
  const auto r              = node.get_attr<FloatAttribute>(A_R);
  const auto nsteps         = node.get_attr<IntAttribute>(A_NSTEPS);
  const auto scale          = node.get_attr<FloatAttribute>(A_SCALE);
  const auto center         = node.get_attr<Vec2FloatAttribute>(A_CENTER);
  const auto outer_slope    = node.get_attr<FloatAttribute>(A_OUTER_SLOPE);
  const auto elevation_exp  = node.get_attr<FloatAttribute>(A_ELEVATION_EXP);
  const auto shape_gain     = node.get_attr<FloatAttribute>(A_SHAPE_GAIN);
  const auto seed           = node.get_attr<SeedAttribute>(A_SEED);
  const auto kw             = node.get_attr<WaveNbAttribute>(A_KW);
  const auto noise_amp      = node.get_attr<FloatAttribute>(A_NOISE_AMP);
  const auto noise_rugosity = node.get_attr<FloatAttribute>(A_NOISE_RUGOSITY);
  const auto noise_inflate  = node.get_attr<BoolAttribute>(A_NOISE_INFLATE);
  // clang-format on

  // --- Compute

  if (p_dx || p_dy)
  {
    // --- External noise inputs

    hmap::for_each_tile(
        {p_dx, p_dy, p_control},
        {p_out},
        [&](std::vector<const hmap::Array *> in,
            std::vector<hmap::Array *>       out,
            const hmap::TileRegion          &region)
        {
          auto [pa_dx, pa_dy, pa_control] = unpack<3>(in);
          auto [pa_out] = unpack<1>(out);

          *pa_out = hmap::multisteps(region.shape,
                                     angle,
                                     r,
                                     nsteps,
                                     elevation_exp,
                                     shape_gain,
                                     scale,
                                     outer_slope,
                                     pa_control,
                                     pa_dx,
                                     pa_dy,
                                     center,
                                     region.bbox);
        },
        node.cfg().cm_cpu);
  }
  else
  {
    // --- Built-in procedural noise

    hmap::for_each_tile(
        {p_control},
        {p_out},
        [&](std::vector<const hmap::Array *> in,
            std::vector<hmap::Array *>       out,
            const hmap::TileRegion          &region)
        {
          auto [pa_control] = unpack<1>(in);

          auto [pa_out] = unpack<1>(out);

          *pa_out = hmap::gpu::multisteps(region.shape,
                                          angle,
                                          seed,
                                          kw,
                                          noise_amp,
                                          noise_rugosity,
                                          noise_inflate,
                                          r,
                                          nsteps,
                                          elevation_exp,
                                          shape_gain,
                                          scale,
                                          outer_slope,
                                          pa_control,
                                          center,
                                          region.bbox);
        },
        node.cfg().cm_gpu);
  }

  // --- Post-process

  post_apply_enveloppe(node, *p_out, p_envelope);
  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
