/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/synthesis.hpp"
#include "highmap/virtual_array/virtual_texture.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_HEIGHTMAP        = "heightmap";
constexpr const char *P_HEIGHTMAP_GUIDE_ = "heightmap (guide)";
constexpr const char *P_TEXTURE          = "texture";
constexpr const char *P_TEXTURE_GUIDE_   = "texture (guide)";
constexpr const char *P_TEXTURE_A        = "texture A";
constexpr const char *P_TEXTURE_A_OUT    = "texture A out";
constexpr const char *P_TEXTURE_B        = "texture B";
constexpr const char *P_TEXTURE_B_OUT    = "texture B out";
constexpr const char *P_TEXTURE_C        = "texture C";
constexpr const char *P_TEXTURE_C_OUT    = "texture C out";
constexpr const char *P_TEXTURE_D        = "texture D";
constexpr const char *P_TEXTURE_D_OUT    = "texture D out";

constexpr const char *A_FILTER_WIDTH_RATIO = "filter_width_ratio";
constexpr const char *A_OVERLAP            = "overlap";
constexpr const char *A_PATCH_FLIP         = "patch_flip";
constexpr const char *A_PATCH_ROTATE       = "patch_rotate";
constexpr const char *A_PATCH_TRANSPOSE    = "patch_transpose";
constexpr const char *A_PATCH_WIDTH        = "patch_width";
constexpr const char *A_SEED               = "seed";

void setup_texture_quilting_shuffle_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, P_TEXTURE_GUIDE_);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_HEIGHTMAP_GUIDE_);
  node.add_port<hmap::VirtualTexture>(gnode::PortType::OUT, P_TEXTURE, CONFIG_TEX(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_HEIGHTMAP, CONFIG(node));

  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, P_TEXTURE_A);
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, P_TEXTURE_B);
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, P_TEXTURE_C);
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, P_TEXTURE_D);

  node.add_port<hmap::VirtualTexture>(gnode::PortType::OUT,
                                      P_TEXTURE_A_OUT,
                                      CONFIG_TEX(node));
  node.add_port<hmap::VirtualTexture>(gnode::PortType::OUT,
                                      P_TEXTURE_B_OUT,
                                      CONFIG_TEX(node));
  node.add_port<hmap::VirtualTexture>(gnode::PortType::OUT,
                                      P_TEXTURE_C_OUT,
                                      CONFIG_TEX(node));
  node.add_port<hmap::VirtualTexture>(gnode::PortType::OUT,
                                      P_TEXTURE_D_OUT,
                                      CONFIG_TEX(node));

  // attribute(s)
  add_float(node, A_PATCH_WIDTH, "patch_width", 0.3f, 0.1f, 1.f);
  add_float(node, A_OVERLAP, "overlap", 0.9f, 0.05f, 0.95f);
  add_seed(node, A_SEED, "Seed");
  add_bool(node, A_PATCH_FLIP, "patch_flip", true);
  add_bool(node, A_PATCH_ROTATE, "patch_rotate", true);
  add_bool(node, A_PATCH_TRANSPOSE, "patch_transpose", true);
  add_float(node, A_FILTER_WIDTH_RATIO, "filter_width_ratio", 0.5f, 0.f, 1.f);
}

void compute_texture_quilting_shuffle_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualTexture *p_texture_guide = node.get_value_ref<hmap::VirtualTexture>(
      P_TEXTURE_GUIDE_);
  hmap::VirtualArray *p_hmap_guide = node.get_value_ref<hmap::VirtualArray>(
      P_HEIGHTMAP_GUIDE_);

  if (p_texture_guide)
  {

    hmap::VirtualArray *p_hmap_out = node.get_value_ref<hmap::VirtualArray>(P_HEIGHTMAP);
    hmap::VirtualTexture *p_texture_out = node.get_value_ref<hmap::VirtualTexture>(
        P_TEXTURE);

    hmap::VirtualTexture *p_tex_a = node.get_value_ref<hmap::VirtualTexture>(P_TEXTURE_A);
    hmap::VirtualTexture *p_tex_b = node.get_value_ref<hmap::VirtualTexture>(P_TEXTURE_B);
    hmap::VirtualTexture *p_tex_c = node.get_value_ref<hmap::VirtualTexture>(P_TEXTURE_C);
    hmap::VirtualTexture *p_tex_d = node.get_value_ref<hmap::VirtualTexture>(P_TEXTURE_D);

    hmap::VirtualTexture *p_tex_out_a = node.get_value_ref<hmap::VirtualTexture>(
        P_TEXTURE_A_OUT);
    hmap::VirtualTexture *p_tex_out_b = node.get_value_ref<hmap::VirtualTexture>(
        P_TEXTURE_B_OUT);
    hmap::VirtualTexture *p_tex_out_c = node.get_value_ref<hmap::VirtualTexture>(
        P_TEXTURE_C_OUT);
    hmap::VirtualTexture *p_tex_out_d = node.get_value_ref<hmap::VirtualTexture>(
        P_TEXTURE_D_OUT);

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

    // hmap::VirtualArray *p_out =
    // node.get_value_ref<hmap::VirtualArray>(P_HEIGHTMAP);

    int ir = std::max(1, (int)(node.val<float>(A_PATCH_WIDTH) * p_hmap_out->shape.x));
    glm::ivec2 patch_base_shape = glm::ivec2(ir, ir);

    // --- work on a single array (i.e. not-tiled algo)

    hmap::Array out_array = hmap::quilting_shuffle(guide_array,
                                                   patch_base_shape,
                                                   node.val<float>(A_OVERLAP),
                                                   node.val<int>(A_SEED),
                                                   secondary_arrays_ptr,
                                                   node.val<bool>(A_PATCH_FLIP),
                                                   node.val<bool>(A_PATCH_ROTATE),
                                                   node.val<bool>(A_PATCH_TRANSPOSE),
                                                   node.val<float>(A_FILTER_WIDTH_RATIO));

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
