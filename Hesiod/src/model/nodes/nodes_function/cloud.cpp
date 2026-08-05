/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>

#include "highmap/colorize.hpp"
#include "highmap/geometry/cloud.hpp"
#include "highmap/operator.hpp"
#include "highmap/virtual_array/virtual_array.hpp"

#include "meta/core/data_provider.hpp"
#include "meta/metadata/keys.hpp"
#include "meta_qt/widgets/points_canvas.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/compat_attributes.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_BACKGROUND = "background";
constexpr const char *P_OUT = "cloud";

constexpr const char *A_CLOUD = "cloud";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_cloud_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_BACKGROUND);
  node.add_port<hmap::Cloud>(gnode::PortType::OUT, P_OUT);

  // --- Attributes

  auto &c = node.meta_group().current();

  auto *a = c.add<std::vector<glm::vec3>>(A_CLOUD, {});
  a->metadata().try_add(meta::keys::ui::label, std::string("Cloud"));
  a->metadata().try_add(meta::keys::ui::widget_type, std::string("PointsEditor"));
  a->metadata().try_add(meta::keys::ui::category, std::string("Main"));
  a->metadata().try_add(std::string(hsd::compat::keys::type_label),
                         std::string("Cloud"));
  a->metadata().try_add(
      meta::keys::ui::data_provider,
      meta::DataProvider{
          [&node, port_id = std::string(P_BACKGROUND)]() -> meta::Any
          {
            hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(port_id);
            if (!p_in)
              return {};
            const glm::ivec2 shape(256, 256);
            hmap::Array array = p_in->to_array(shape, node.cfg().cm_cpu);
            std::vector<uint8_t> img =
                hmap::colorize(array, array.min(), array.max(), hmap::Cmap::MAGMA, false)
                    .to_img_8bit();
            meta::qt::ImageData d;
            d.width = shape.x;
            d.height = shape.y;
            d.channels = 3; // to_img_8bit() -> RGB
            // vertical flip so the thumbnail origin matches the canvas (legacy mirrored(false,true))
            const int stride = shape.x * 3;
            d.pixels.resize(img.size());
            for (int y = 0; y < shape.y; ++y)
              std::copy_n(img.data() + (shape.y - 1 - y) * stride,
                          stride,
                          d.pixels.data() + y * stride);
            return d;
          }});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_cloud_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_out = node.get_value_ref<hmap::Cloud>(P_OUT);

  // --- Params

  const auto cloud_attr =
      node.meta_group().current().value<std::vector<glm::vec3>>(A_CLOUD);

  // --- Compute

  *p_out = hmap::Cloud(cloud_attr);
}

} // namespace hesiod
