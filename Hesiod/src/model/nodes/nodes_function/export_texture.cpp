/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/export.hpp"
#include "highmap/virtual_array/virtual_texture.hpp"

#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/node_factory.hpp"
#include "hesiod/model/nodes/post_process.hpp"
#include "hesiod/model/utils.hpp"

using namespace attr;

namespace hesiod
{

void setup_export_texture_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, "texture");

  // attribute(s)
  node.add_attr<FilenameAttribute>("fname",
                                   "fname",
                                   std::filesystem::path("texture.png"),
                                   "PNG (*.png)",
                                   true);
  node.add_attr<BoolAttribute>("16 bit", "16 bit", false);
  node.add_attr<BoolAttribute>("auto_export", "Auto Export on Node Update", false);
  node.add_attr<BoolAttribute>("add_prefix", "Add Project Name as Prefix", false);

  // attribute(s) order
  node.set_attr_ordered_key({"fname", "16 bit", "auto_export", "add_prefix"});

  // specialized GUI
}

void compute_export_texture_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualTexture *p_in = node.get_value_ref<hmap::VirtualTexture>("texture");

  if (p_in && node.get_attr<BoolAttribute>("auto_export"))
  {
    std::filesystem::path fname = node.get_attr<FilenameAttribute>("fname");
    fname = ensure_extension(fname, ".png");

    if (node.get_attr<BoolAttribute>("add_prefix"))
      fname = prepend_project_name_to_path(fname);

    if (node.get_attr<BoolAttribute>("16 bit"))
      p_in->to_png(fname.string(), node.cfg().cm_cpu, CV_16U);
    else
      p_in->to_png(fname.string(), node.cfg().cm_cpu, CV_8U);
  }
}

} // namespace hesiod
