/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <fstream>

#include "highmap/texture.hpp"
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
constexpr const char *P_TEXTURE = "texture";

constexpr const char *A_SIZE = "size";

void setup_texture_uv_checker_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualTexture>(gnode::PortType::OUT, P_TEXTURE, CONFIG_TEX(node));

  // attribute(s)
  std::vector<std::string> choices = {"4x4", "8x8", "16x16"};
  add_choice(node, A_SIZE, "size", choices, "8x8");
}

void compute_texture_uv_checker_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualTexture *p_out = node.get_value_ref<hmap::VirtualTexture>(P_TEXTURE);

  // generated with https://uvchecker.vinzi.xyz/

  std::string fname;

  if (node.val<std::string>(A_SIZE) == "4x4")
    fname = "data/uv_checker_2k_04x04.png";
  else if (node.val<std::string>(A_SIZE) == "8x8")
    fname = "data/uv_checker_2k_08x08.png";
  else if (node.val<std::string>(A_SIZE) == "16x16")
    fname = "data/uv_checker_2k_16x16.png";

  // if the file exists, keep going
  std::ifstream f(fname.c_str());
  if (f.good())
  {
    // load rgba data
    bool          flip_j = true;
    hmap::Texture tensor4(fname, flip_j);
    tensor4 = tensor4.resample_to_shape(node.cfg().shape);

    for (int nch = 0; nch < 4; ++nch)
      p_out->channel(nch).from_array(tensor4.channel(nch), node.cfg().cm_cpu);
  }
}

} // namespace hesiod
