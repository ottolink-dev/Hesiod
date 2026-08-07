/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/synthesis.hpp"
#include "highmap/virtual_array/virtual_texture.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_texture_quilting_expand_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, "texture (guide)");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "heightmap (guide)");
  node.add_port<hmap::VirtualTexture>(gnode::PortType::OUT, "texture", CONFIG_TEX(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "heightmap", CONFIG(node));

  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, "texture A");
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, "texture B");
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, "texture C");
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, "texture D");

  node.add_port<hmap::VirtualTexture>(gnode::PortType::OUT,
                                      "texture A out",
                                      CONFIG_TEX(node));
  node.add_port<hmap::VirtualTexture>(gnode::PortType::OUT,
                                      "texture B out",
                                      CONFIG_TEX(node));
  node.add_port<hmap::VirtualTexture>(gnode::PortType::OUT,
                                      "texture C out",
                                      CONFIG_TEX(node));
  node.add_port<hmap::VirtualTexture>(gnode::PortType::OUT,
                                      "texture D out",
                                      CONFIG_TEX(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("expansion_ratio", "expansion_ratio", 2.f, 1.f, FLT_MAX);
  node.add_attr<FloatAttribute>("patch_width", "patch_width", 0.3f, 0.1f, 1.f);
  node.add_attr<FloatAttribute>("overlap", "overlap", 0.9f, 0.05f, 0.95f);
  node.add_attr<SeedAttribute>("seed", "Seed");
  node.add_attr<BoolAttribute>("patch_flip", "patch_flip", true);
  node.add_attr<BoolAttribute>("patch_rotate", "patch_rotate", true);
  node.add_attr<BoolAttribute>("patch_transpose", "patch_transpose", true);
  node.add_attr<FloatAttribute>("filter_width_ratio",
                                "filter_width_ratio",
                                0.5f,
                                0.f,
                                1.f);

  // attribute(s) order
  node.set_attr_ordered_key({"expansion_ratio",
                             "patch_width",
                             "overlap",
                             "seed",
                             "patch_flip",
                             "patch_rotate",
                             "patch_transpose",
                             "filter_width_ratio"});
}

void compute_texture_quilting_expand_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualTexture *p_texture_guide = node.get_value_ref<hmap::VirtualTexture>(
      "texture (guide)");
  hmap::VirtualArray *p_hmap_guide = node.get_value_ref<hmap::VirtualArray>(
      "heightmap (guide)");

  if (p_texture_guide)
  {

    hmap::VirtualArray *p_hmap_out = node.get_value_ref<hmap::VirtualArray>("heightmap");
    hmap::VirtualTexture *p_texture_out = node.get_value_ref<hmap::VirtualTexture>(
        "texture");

    hmap::VirtualTexture *p_tex_a = node.get_value_ref<hmap::VirtualTexture>("texture A");
    hmap::VirtualTexture *p_tex_b = node.get_value_ref<hmap::VirtualTexture>("texture B");
    hmap::VirtualTexture *p_tex_c = node.get_value_ref<hmap::VirtualTexture>("texture C");
    hmap::VirtualTexture *p_tex_d = node.get_value_ref<hmap::VirtualTexture>("texture D");

    hmap::VirtualTexture *p_tex_out_a = node.get_value_ref<hmap::VirtualTexture>(
        "texture A out");
    hmap::VirtualTexture *p_tex_out_b = node.get_value_ref<hmap::VirtualTexture>(
        "texture B out");
    hmap::VirtualTexture *p_tex_out_c = node.get_value_ref<hmap::VirtualTexture>(
        "texture C out");
    hmap::VirtualTexture *p_tex_out_d = node.get_value_ref<hmap::VirtualTexture>(
        "texture D out");

    // for the guide array used to defined the quilting parameters,
    // use the heightmap if available. If not, use the texture luminance
    hmap::Array guide_array;

    if (p_hmap_guide)
      guide_array = p_hmap_guide->to_array(node.cfg().cm_cpu);
    else
    {
      hmap::VirtualArray luminance(CONFIG(node));
      guide_array = luminance.to_array(node.cfg().cm_cpu);
    }

    // --- define secondary arrays

    std::vector<hmap::Array>   secondary_arrays_storage = {};
    std::vector<hmap::Array *> secondary_arrays_ptr     = {};

    // add first each RGBA components that are quilted based on the
    // luminance and then add other secondary textures (also modified
    // based on the guide texture luminance)
    for (auto ptr : {p_texture_guide, p_tex_a, p_tex_b, p_tex_c, p_tex_d})
      if (ptr)
      {
        for (int nch = 0; nch < ptr->channels(); ++nch)
          secondary_arrays_storage.push_back(
              ptr->channel(nch).to_array(node.cfg().cm_cpu));
      }

    // create vector of reference ptrs
    for (auto &v : secondary_arrays_storage)
      secondary_arrays_ptr.push_back(&v);

    int ir = std::max(
        1,
        (int)(node.get_attr<FloatAttribute>("patch_width") * p_hmap_out->shape.x));
    glm::ivec2 patch_base_shape = glm::ivec2(ir, ir);

    // --- work on a single array (i.e. not-tiled algo)

    hmap::Array out_array = hmap::quilting_expand(
        guide_array,
        node.get_attr<FloatAttribute>("expansion_ratio"),
        patch_base_shape,
        node.get_attr<FloatAttribute>("overlap"),
        node.get_attr<SeedAttribute>("seed"),
        secondary_arrays_ptr,
        true, // keep_input_shape
        node.get_attr<BoolAttribute>("patch_flip"),
        node.get_attr<BoolAttribute>("patch_rotate"),
        node.get_attr<BoolAttribute>("patch_transpose"),
        node.get_attr<FloatAttribute>("filter_width_ratio"));

    // rebuild outputs
    p_hmap_out->from_array(out_array, node.cfg().cm_cpu);

    // textures
    std::vector<hmap::VirtualTexture *> pt_in_vec  = {p_texture_guide,
                                                      p_tex_a,
                                                      p_tex_b,
                                                      p_tex_c,
                                                      p_tex_d};
    std::vector<hmap::VirtualTexture *> pt_out_vec = {p_texture_out,
                                                      p_tex_out_a,
                                                      p_tex_out_b,
                                                      p_tex_out_c,
                                                      p_tex_out_d};

    // storage index within the secondary_arrays_ptr vector
    int current_idx = 0;

    for (size_t k = 0; k < pt_in_vec.size(); k++)
      if (pt_in_vec[k])
      {
        // rebuild the texture from Heightmap
        hmap::VirtualArray r(CONFIG(node));
        hmap::VirtualArray g(CONFIG(node));
        hmap::VirtualArray b(CONFIG(node));
        hmap::VirtualArray a(CONFIG(node));

        for (int nch = 0; nch < 4; ++nch)
          pt_out_vec[k]->channel(nch).from_array(*secondary_arrays_ptr[current_idx + nch],
                                                 node.cfg().cm_cpu);

        current_idx += 4;
      }
  }
}

} // namespace hesiod
