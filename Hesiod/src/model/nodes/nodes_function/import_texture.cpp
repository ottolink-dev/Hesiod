/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <fstream>

#include "highmap/tensor.hpp"
#include "highmap/virtual_array/virtual_texture.hpp"

#include "attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_import_texture_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualTexture>(gnode::PortType::OUT, "texture", CONFIG_TEX(node));

  // attribute(s)
  node.add_attr<FilenameAttribute>("fname",
                                   "fname",
                                   std::filesystem::path(""),
                                   "Image files (*.bmp *.dib *.jpeg *.jpg *.png *.pbm "
                                   "*.pgm *.ppm *.pxm *.pnm *.tiff *.tif *.hdr *.pic)",
                                   false);
  node.add_attr<BoolAttribute>("flip_y", "flip_y", true);

  // attribute(s) order
  node.set_attr_ordered_key({"fname", "flip_y"});
}

void compute_import_texture_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualTexture *p_tex = node.get_value_ref<hmap::VirtualTexture>("texture");

  std::string fname = node.get_attr<FilenameAttribute>("fname").string();

  // if the file exists, keep going
  std::ifstream f(fname.c_str());
  if (f.good())
  {
    // load rgba data
    hmap::Tensor tensor4(fname, node.get_attr<BoolAttribute>("flip_y"));

    if (tensor4.shape.x * tensor4.shape.y == 0)
    {
      Logger::log()->error("compute_import_texture_node: Failed to construct Tensor");
      return;
    }

    tensor4 = tensor4.resample_to_shape_xy(node.cfg().shape);

    hmap::Array ra = tensor4.get_slice(0);
    hmap::Array ga = tensor4.get_slice(1);
    hmap::Array ba = tensor4.get_slice(2);
    hmap::Array aa = tensor4.get_slice(3);

    p_tex->from_arrays({&ra, &ga, &ba, &aa}, node.cfg().cm_cpu);
  }
}

} // namespace hesiod
