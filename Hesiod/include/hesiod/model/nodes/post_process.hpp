/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once

#include "hesiod/model/nodes/base_node.hpp"

namespace hesiod
{

struct PostProcessHeightmapOptions
{
  bool add_mix = false;
  bool remap_active_state = true;
};

void blend_heightmaps(BaseNode           &node,
                      hmap::VirtualArray &h_out,
                      hmap::VirtualArray &h1,
                      hmap::VirtualArray &h2,
                      BlendingMethod      method,
                      float               k = 0.f,
                      int                 ir = 0,
                      float               w1 = 1.f,
                      float               w2 = 1.f);

void post_apply_enveloppe(BaseNode           &node,
                          hmap::VirtualArray &h,
                          hmap::VirtualArray *p_env);

void post_apply_saturate_percentile(BaseNode           &node,
                                    hmap::VirtualArray &h,
                                    float               satmin,
                                    float               satmax);

void post_process_heightmap(BaseNode           &node,
                            hmap::VirtualArray &h,
                            hmap::VirtualArray *p_in = nullptr);

void setup_post_process_heightmap_attributes(BaseNode                   &node,
                                             PostProcessHeightmapOptions options);

// --- mask preprocessing

std::shared_ptr<hmap::VirtualArray> pre_process_mask(BaseNode            &node,
                                                     hmap::VirtualArray *&p_mask,
                                                     hmap::VirtualArray  &h);

void setup_pre_process_mask_attributes(BaseNode &node);

// --- default noise

struct DefaultNoiseOptions
{
  bool        add_default_noise = true;
  float       noise_amp = 0.1f;
  float       kw = 2.f;
  float       smoothness = 0.f;
  std::string noise_type = "OpenSimplex2";
};

void generate_noise(BaseNode            &node,
                    hmap::VirtualArray *&p_noise,
                    hmap::VirtualArray  &noise,
                    uint                 seed_increment = 0);
void setup_default_noise(BaseNode &node, const DefaultNoiseOptions &options);

// --- default map

struct DefaultMapOptions
{
  enum Type : int
  {
    DMT_UNIFORM_ONE,
    DMT_PULSE
  } map_type = Type::DMT_UNIFORM_ONE;

  bool inverse_output = false;

  static std::map<std::string, int> type_map()
  {
    return {{"Uniform", DefaultMapOptions::Type::DMT_UNIFORM_ONE},
            {"Cubic Pulse", DefaultMapOptions::Type::DMT_PULSE}};
  }
};

void generate_map(BaseNode                &node,
                  hmap::VirtualArray     *&p_map,
                  hmap::VirtualArray      &map,
                  const DefaultMapOptions &options);

} // namespace hesiod
