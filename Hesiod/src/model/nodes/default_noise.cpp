/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/primitives.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

void generate_noise(BaseNode            &node,
                    hmap::VirtualArray *&p_noise,
                    hmap::VirtualArray  &noise,
                    uint                 seed_increment)
{
  if (p_noise || !node.val<bool>("dn_add_default_noise"))
    return;

  hmap::for_each_tile(
      {&noise},
      [&node, seed_increment](std::vector<hmap::Array *> p_arrays,
                              const hmap::TileRegion    &region)
      {
        auto [pa_noise_default] = unpack<1>(p_arrays);

        float     kw_x = node.val<float>("dn_kw");
        glm::vec2 kw = {kw_x, kw_x};

        auto ntype = hmap::NoiseType(node.val<int>("dn_noise_type"));
        uint seed = node.val<int>("dn_seed") + seed_increment;

        *pa_noise_default = hmap::gpu::noise_fbm(ntype,
                                                 region.shape,
                                                 kw,
                                                 seed,
                                                 8,
                                                 node.val<float>("dn_smoothness"),
                                                 0.5f,
                                                 2.f,
                                                 nullptr,
                                                 nullptr,
                                                 nullptr,
                                                 region.bbox);

        *pa_noise_default *= node.val<float>("dn_noise_amp");
      },
      node.cfg().cm_gpu);

  p_noise = &noise;
}

void setup_default_noise(BaseNode &node, const DefaultNoiseOptions &options)
{
  node.set_current_category("Default Noise");

  // clang-format off
  add_bool(node, "dn_add_default_noise", "Activate", options.add_default_noise);
  add_enum(node, "dn_noise_type", "Type", enum_mappings.noise_type_map_fbm, options.noise_type);
  add_seed(node, "dn_seed", "Seed");
  add_float(node, "dn_noise_amp", "Amplitude", options.noise_amp, 0.f, FLT_MAX);
  add_float(node, "dn_kw", "Spatial Frequency", options.kw, 0.f, FLT_MAX);
  add_float(node, "dn_smoothness", "Smoothness", options.smoothness, 0.f, 1.f);
  // clang-format on
}

} // namespace hesiod
