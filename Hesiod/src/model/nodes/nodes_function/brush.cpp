/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_BACKGROUND = "background";
constexpr const char *P_OUT        = "out";

void setup_brush_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_BACKGROUND);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  add_array(node, "hmap", "HeightMap");

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = true});

  // background thumbnail behind the paint canvas (ImageData data_provider;
  // helper's meta path is attribute-generic despite the cloud name)
  setup_background_image_for_cloud_attribute(node, "hmap", "background");
}

void compute_brush_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  // retrieve raw data and convert them to an hmap::Array
  const auto arr = node.get_meta_group().current().value<meta::Array>("hmap");

  hmap::Array array(arr.shape);
  array.vector = arr.vector;
  array        = array.resample_to_shape_bilinear(node.get_config_ref()->shape);

  // Array -> VirtualArray
  p_out->from_array(array, node.cfg().cm_cpu);

  // post-process
  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
