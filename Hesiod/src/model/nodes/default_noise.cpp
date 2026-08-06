/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/primitives.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void generate_noise(BaseNode            &node,
                    hmap::VirtualArray *&p_noise,
                    hmap::VirtualArray  &noise,
                    uint                 seed_increment)
{
  if (!p_noise && node.get_attr<BoolAttribute>("dn_add_default_noise"))
  {
    hmap::for_each_tile(
        {&noise},
        [&node, seed_increment](std::vector<hmap::Array *> p_arrays,
                                const hmap::TileRegion    &region)
        {
          auto [pa_noise_default] = unpack<1>(p_arrays);

          glm::vec2 kw = {node.get_attr<FloatAttribute>("dn_kw"),
                          node.get_attr<FloatAttribute>("dn_kw")};

          auto ntype = (hmap::NoiseType)node.get_attr<EnumAttribute>("dn_noise_type");
          uint seed = node.get_attr<SeedAttribute>("dn_seed") + seed_increment;

          *pa_noise_default = hmap::gpu::noise_fbm(
              ntype,
              region.shape,
              kw,
              seed,
              8,
              node.get_attr<FloatAttribute>("dn_smoothness"),
              0.5f,
              2.f,
              nullptr,
              nullptr,
              nullptr,
              nullptr,
              region.bbox);

          *pa_noise_default *= node.get_attr<FloatAttribute>("dn_noise_amp");
        },
        node.cfg().cm_gpu);

    p_noise = &noise;
  }
}

void setup_default_noise(BaseNode &node, const DefaultNoiseOptions &options)
{
  node.add_attr<BoolAttribute>("dn_add_default_noise",
                               "Activate",
                               options.add_default_noise);
  node.add_attr<EnumAttribute>("dn_noise_type",
                               "Type",
                               enum_mappings.noise_type_map_fbm,
                               options.noise_type);
  node.add_attr<SeedAttribute>("dn_seed", "Seed");
  node.add_attr<FloatAttribute>("dn_noise_amp",
                                "Amplitude",
                                options.noise_amp,
                                0.f,
                                FLT_MAX);
  node.add_attr<FloatAttribute>("dn_kw", "Spatial Frequency", options.kw, 0.f, FLT_MAX);
  node.add_attr<FloatAttribute>("dn_smoothness",
                                "Smoothness",
                                options.smoothness,
                                0.f,
                                1.f);

  std::vector<std::string> *p_keys = node.get_attr_ordered_key_ref();

  p_keys->push_back("_GROUPBOX_BEGIN_Default noise");
  p_keys->push_back("dn_add_default_noise");
  p_keys->push_back("dn_noise_type");
  p_keys->push_back("dn_seed");
  p_keys->push_back("dn_noise_amp");
  p_keys->push_back("dn_kw");
  p_keys->push_back("dn_smoothness");
  p_keys->push_back("_GROUPBOX_END_");
}

} // namespace hesiod
