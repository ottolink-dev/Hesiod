/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <fstream>

#include "highmap/range.hpp"

#include "attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_OUT = "output";

constexpr const char *A_FILENAME = "fname";
constexpr const char *A_FLIP_Y = "flip_y";
constexpr const char *A_CLIP_RANGE = "clip_range";
constexpr const char *A_SAMPLING_METHOD = "sampling_method";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_import_heightmap_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Attributes

  std::vector<std::string> choices = {"Bicubic", "Nearest"};

  // clang-format off
  node.add_attr<FilenameAttribute>(A_FILENAME, "Filename", std::filesystem::path(""), "Image files (*.bmp *.dib *.jpeg *.jpg *.png *.pbm *.pgm *.ppm *.pxm *.pnm *.tiff *.tif *.hdr *.pic *.exr)", false);
  node.add_attr<BoolAttribute>(A_FLIP_Y, "Flip Y", true);
  node.add_attr<BoolAttribute>(A_CLIP_RANGE, "Clip Range", false);
  node.add_attr<ChoiceAttribute>(A_SAMPLING_METHOD, "Sampling Method", choices);
  // clang-format on

  // --- Attribute(s) order

  node.set_attr_ordered_key({"_GROUPBOX_BEGIN_Import",
                             A_FILENAME,
                             A_FLIP_Y,
                             A_CLIP_RANGE,
                             A_SAMPLING_METHOD,
                             "_GROUPBOX_END_"});

  setup_post_process_heightmap_attributes(
      node,
      {.add_mix = false, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_import_heightmap_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Outputs

  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_out)
    return;

  // --- Params

  // clang-format off
  const auto fname           = node.get_attr<FilenameAttribute>(A_FILENAME);
  const auto flip_y          = node.get_attr<BoolAttribute>(A_FLIP_Y);
  const auto clip_range      = node.get_attr<BoolAttribute>(A_CLIP_RANGE);
  const auto sampling_method = node.get_attr<ChoiceAttribute>(A_SAMPLING_METHOD);
  // clang-format on

  // --- Sanity check

  {
    std::ifstream f(fname.string().c_str());

    if (!f.good())
    {
      Logger::log()->error("compute_import_heightmap_node: could not load image file {}",
                           fname.string());
      return;
    }
  }

  // --- Import

  hmap::Array z(fname.string(), flip_y);

  // --- Resample

  if (sampling_method == "Bicubic")
    z = z.resample_to_shape_bicubic(p_out->shape);
  else if (sampling_method == "Nearest")
    z = z.resample_to_shape_nearest(p_out->shape);

  // --- Upload

  p_out->from_array(z, node.cfg().cm_cpu);

  // --- Clamp interpolation overshoots

  if (clip_range)
  {
    hmap::for_each_tile(
        {},
        {p_out},
        [&](std::vector<const hmap::Array *>,
            std::vector<hmap::Array *> out,
            const hmap::TileRegion &)
        {
          auto [pa_out] = unpack<1>(out);

          hmap::clamp(*pa_out, 0.f, 1.f);
        },
        node.cfg().cm_cpu);
  }

  // --- Post-process

  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
