/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <cmath>
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
  node.add_attr<BoolAttribute>("keep_aspect_ratio", "keep_aspect_ratio", false);

  // attribute(s) order
  node.set_attr_ordered_key({"fname", "flip_y", "keep_aspect_ratio"});
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

    glm::ivec2 target = node.cfg().shape;

    if (node.get_attr<BoolAttribute>("keep_aspect_ratio"))
    {
      // fit the source within the target shape while preserving its aspect
      // ratio, then centre it with transparent padding (alpha = 0)
      float scale = std::min((float)target.x / (float)tensor4.shape.x,
                             (float)target.y / (float)tensor4.shape.y);

      glm::ivec2 fitted = {std::max(1, (int)std::round(tensor4.shape.x * scale)),
                           std::max(1, (int)std::round(tensor4.shape.y * scale))};

      hmap::Tensor fitted_tensor = tensor4.resample_to_shape_xy(fitted);

      int ox = (target.x - fitted.x) / 2;
      int oy = (target.y - fitted.y) / 2;

      hmap::Array ra(target, 0.f);
      hmap::Array ga(target, 0.f);
      hmap::Array ba(target, 0.f);
      hmap::Array aa(target, 0.f);

      hmap::Array f_r = fitted_tensor.get_slice(0);
      hmap::Array f_g = fitted_tensor.get_slice(1);
      hmap::Array f_b = fitted_tensor.get_slice(2);
      hmap::Array f_a = fitted_tensor.get_slice(3);

      for (int i = 0; i < fitted.x; ++i)
        for (int j = 0; j < fitted.y; ++j)
        {
          ra(ox + i, oy + j) = f_r(i, j);
          ga(ox + i, oy + j) = f_g(i, j);
          ba(ox + i, oy + j) = f_b(i, j);
          aa(ox + i, oy + j) = f_a(i, j);
        }

      p_tex->from_arrays({&ra, &ga, &ba, &aa}, node.cfg().cm_cpu);
    }
    else
    {
      tensor4 = tensor4.resample_to_shape_xy(target);

      hmap::Array ra = tensor4.get_slice(0);
      hmap::Array ga = tensor4.get_slice(1);
      hmap::Array ba = tensor4.get_slice(2);
      hmap::Array aa = tensor4.get_slice(3);

      p_tex->from_arrays({&ra, &ga, &ba, &aa}, node.cfg().cm_cpu);
    }
  }
}

} // namespace hesiod
