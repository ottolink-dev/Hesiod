/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "meta/core/data_provider.hpp"
#include "meta/ext/array/array.hpp"
#include "meta/metadata/keys.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/compat_attributes.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

void setup_brush_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "background");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "out", CONFIG(node));

  // attribute(s)
  auto &c = node.get_meta_group().current();

  auto *a = c.add<meta::Array>(
      "hmap",
      meta::Array{glm::ivec2(512, 512), std::vector<float>(512 * 512, 0.f)});
  a->metadata().try_add(meta::keys::ui::label, std::string("Heightmap"));
  a->metadata().try_add(meta::keys::ui::category, std::string("Main"));
  a->metadata().try_add(meta::keys::ui::width, 256);
  a->metadata().try_add(meta::keys::ui::height, 256);
  a->metadata().try_add(std::string(hsd::legacy::keys::type_label), std::string("Array"));

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = true});

  // background thumbnail behind the paint canvas (ImageData data_provider;
  // helper's meta path is attribute-generic despite the cloud name)
  setup_background_image_for_cloud_attribute(node, "hmap", "background");
}

void compute_brush_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("out");

  // retrieve raw data and convert them to an hmap::Array
  const auto arr = node.get_meta_group().current().value<meta::Array>("hmap");

  hmap::Array array(arr.shape);
  array.vector = arr.vector;
  array = array.resample_to_shape_bilinear(node.get_config_ref()->shape);

  // Array -> VirtualArray
  p_out->from_array(array, node.cfg().cm_cpu);

  // post-process
  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
