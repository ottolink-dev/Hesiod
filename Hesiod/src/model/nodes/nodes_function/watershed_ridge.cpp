/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/erosion.hpp"
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

constexpr const char *P_Z       = "z";
constexpr const char *P_SCALING = "scaling";
constexpr const char *P_NOISE_X = "dx";
constexpr const char *P_NOISE_Y = "dy";
constexpr const char *P_MASK    = "mask";
constexpr const char *P_OUTPUT  = "output";

constexpr const char *A_AMPLITUDE       = "amplitude";
constexpr const char *A_WIDTH           = "width";
constexpr const char *A_EDT_EXPONENT    = "edt_exponent";
constexpr const char *A_RADIUS          = "prefilter_radius";
constexpr const char *A_DEFAULT_SCALING = "default_scaling";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_watershed_ridge_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_Z);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_NOISE_X);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_NOISE_Y);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_SCALING);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUTPUT, CONFIG(node));

  // attribute(s)
  add_float(node, A_AMPLITUDE, "amplitude", 0.05f, 0.f, 1.f);
  add_float(node, A_WIDTH, "width", 0.1f, 0.f, 0.5f);
  add_float(node, A_EDT_EXPONENT, "edt_exponent", 0.5f, 0.01f, 2.f);
  add_float(node, A_RADIUS, "Prefilter Radius", 0.2f, 0.f, 0.5f);
  add_bool(node, A_DEFAULT_SCALING, "Enable Default Scaling", true);

  setup_default_noise(node, {.noise_amp = 0.05f, .kw = 8.f, .smoothness = 0.2f});
  setup_pre_process_mask_attributes(node);
  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_watershed_ridge_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  auto *p_in = node.get_value_ref<hmap::VirtualArray>(P_Z);

  if (!p_in)
    return;

  auto *p_dx      = node.get_value_ref<hmap::VirtualArray>(P_NOISE_X);
  auto *p_dy      = node.get_value_ref<hmap::VirtualArray>(P_NOISE_Y);
  auto *p_mask    = node.get_value_ref<hmap::VirtualArray>(P_MASK);
  auto *p_scaling = node.get_value_ref<hmap::VirtualArray>(P_SCALING);
  auto *p_out     = node.get_value_ref<hmap::VirtualArray>(P_OUTPUT);

  // --- Parameters wrapper

  const auto params = [&node, p_out]()
  {
    struct P
    {
      float amplitude;
      float width_pixels;
      float edt_exponent;
      int   ir;
      bool  default_scaling;
    };

    const int   nx     = p_out->shape.x;
    const float width  = node.val<float>(A_WIDTH);
    const float radius = node.val<float>(A_RADIUS);
    const int   ir     = int(radius * nx);

    return P{.amplitude       = node.val<float>(A_AMPLITUDE),
             .width_pixels    = nx * width,
             .edt_exponent    = node.val<float>(A_EDT_EXPONENT),
             .ir              = ir,
             .default_scaling = node.val<bool>(A_DEFAULT_SCALING)};
  }();

  // --- prepare mask

  std::shared_ptr<hmap::VirtualArray> sp_mask = pre_process_mask(node, p_mask, *p_in);

  // --- prepare default scaling

  hmap::VirtualArray default_scaling(CONFIG(node));

  if (!p_scaling && params.default_scaling)
  {
    auto options = DefaultMapOptions{.map_type       = DefaultMapOptions::Type::DMT_PULSE,
                                     .inverse_output = true};
    generate_map(node, p_scaling, default_scaling, options);
  }

  // --- prepare default noise

  hmap::VirtualArray noise_default_x(CONFIG(node));
  hmap::VirtualArray noise_default_y(CONFIG(node));
  uint               seed_increment = 0;
  generate_noise(node, p_dx, noise_default_x, ++seed_increment);
  generate_noise(node, p_dy, noise_default_y, ++seed_increment);

  // --- compute

  float hmin = p_in->min(node.cfg().cm_cpu);
  float hmax = p_in->max(node.cfg().cm_cpu);

  hmap::for_each_tile(
      {p_in, p_dx, p_dy, p_mask, p_scaling},
      {p_out},
      [&node, &params](std::vector<const hmap::Array *> p_arrays_in,
                       std::vector<hmap::Array *>       p_arrays_out,
                       const hmap::TileRegion &)
      {
        auto [pa_in, pa_dx, pa_dy, pa_mask, pa_scaling] = unpack<5>(p_arrays_in);
        auto [pa_out]                                   = unpack<1>(p_arrays_out);

        *pa_out = hmap::gpu::watershed_ridge(*pa_in,
                                             pa_mask,
                                             params.amplitude,
                                             params.width_pixels,
                                             params.edt_exponent,
                                             params.ir,
                                             hmap::FlowDirectionMethod::FDM_D8,
                                             pa_dx,
                                             pa_dy,
                                             pa_scaling);
      },
      node.cfg().cm_single_array);

  p_out->remap(hmin, hmax, node.cfg().cm_cpu);

  // post-process
  p_out->smooth_overlap_buffers();
  post_process_heightmap(node, *p_out, p_in);
}

} // namespace hesiod
