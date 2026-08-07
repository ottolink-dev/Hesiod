/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/virtual_array/virtual_texture.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

void setup_preview_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "elevation");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "water_depth");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "scalar");
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, "texture");
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, "normal map");
  node.add_port<hmap::Cloud>(gnode::PortType::IN, "cloud");
  node.add_port<hmap::Path>(gnode::PortType::IN, "path");
}

void compute_preview_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());
}

} // namespace hesiod
