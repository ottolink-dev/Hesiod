/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "attributes.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"

using namespace attr;

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_IN = "elevation";
constexpr const char *P_TEX = "texture";

constexpr const char *A_PORT = "port";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_blender_bridge_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, P_TEX);

  // --- Attributes

  int current_port = int(HSD_APP->get_blender_streamer().get_port());

  // clang-format off
  node.add_attr<IntAttribute>(A_PORT, "Communication Port", current_port, 1024, 65535);
  // clang-format on

  // --- Attribute(s) order
  node.set_attr_ordered_key({A_PORT});

  HSD_APP->get_blender_streamer().start(HSD_APP->get_blender_streamer().get_port());
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_blender_bridge_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);
  auto *p_tex = node.get_value_ref<hmap::VirtualTexture>(P_TEX);

  if (!p_in)
    return;

  // --- Params

  // clang-format off
  const auto port = std::uint16_t(node.get_attr<IntAttribute>(A_PORT));
  // clang-format on

  // --- Compute

  BlenderStreamer &streamer = HSD_APP->get_blender_streamer();

  // only restart if the port is changed
  streamer.start(port);

  hmap::Array z = p_in->to_array(node.cfg().cm_cpu);
  hmap::remap(z);

  if (!p_tex)
  {
    streamer.send_heightmap(z.vector.data(), node.cfg().shape.x, node.cfg().shape.y);
    return;
  }
  else
  {
    std::vector<float> raw_tex = p_tex->to_raw(node.cfg().cm_cpu);

    streamer.send_heightmap_and_texture(z.vector.data(),
                                        raw_tex.data(),
                                        node.cfg().shape.x,
                                        node.cfg().shape.y);
  }
}

} // namespace hesiod
