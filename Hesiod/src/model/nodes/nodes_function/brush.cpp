/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "meta/metadata/keys.hpp"

#include "hesiod/app/hesiod_application.hpp"
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
constexpr const char *P_OUT        = "output";

void setup_brush_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_BACKGROUND);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  // Storage belongs to the painting, not to the graph output. Loading a saved
  // painting replaces this initial shape without resampling its data.
  const int resolution = HSD_CTX.app_settings.node_editor.brush_storage_resolution;

  auto &paint = add_array(node,
                          "hmap",
                          "HeightMap",
                          {resolution, resolution},
                          {256, 256});
  // Give the canvas its own section in the industrial panel.
  paint.metadata()
      .try_add(std::string(meta::keys::ui::category), std::string("Brush"))
      ->value() = "Brush";

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
  const auto &arr = node.get_meta_group().current().value<meta::Array>("hmap");

  // reject degenerate data (e.g. a zero shape persisted by older releases)
  // instead of resampling an inconsistent array
  hmap::Array array;

  if (arr.shape.x <= 0 || arr.shape.y <= 0 ||
      arr.vector.size() !=
          static_cast<size_t>(arr.shape.x) * static_cast<size_t>(arr.shape.y))
  {
    array = hmap::Array(node.cfg().shape);
    Logger::log()->error(
        "Brush node [{}]: invalid heightmap data (shape {}x{}, {} values), "
        "falling back to a zeroed heightmap",
        node.get_id(),
        arr.shape.x,
        arr.shape.y,
        arr.vector.size());
  }
  else
  {
    hmap::Array painted(arr.shape);
    painted.vector = arr.vector;
    array          = painted.resample_to_shape_bilinear(node.get_config_ref()->shape);
  }

  // Array -> VirtualArray
  p_out->from_array(array, node.cfg().cm_cpu);

  // post-process
  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
