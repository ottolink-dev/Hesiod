/* Copyright (c) 2024 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"

#include "meta/metadata/keys.hpp"
#include "meta/presets/choice.hpp"
#include "meta/presets/color_gradient.hpp"
#include "meta/presets/curve.hpp"
#include "meta/presets/file.hpp"
#include "meta/presets/glm.hpp"
#include "meta/presets/numeric.hpp"
#include "meta/presets/text.hpp"

namespace hesiod
{

static void apply_category_if_set(BaseNode &node, meta::AbstractAttribute &a)
{
  if (!node.get_current_category().empty())
    a.metadata().try_add(std::string(meta::keys::ui::category),
                         std::string(node.get_current_category()));
}

meta::Attribute<float> &add_angle(BaseNode          &node,
                                  const std::string &key,
                                  const std::string &label,
                                  float              default_val,
                                  float              vmin,
                                  float              vmax,
                                  const std::string &value_format)
{
  return add_float(node, key, label, default_val, vmin, vmax, value_format);
}

meta::Attribute<bool> &add_bool(BaseNode          &node,
                                const std::string &key,
                                const std::string &label,
                                bool               default_val)
{
  auto &a = meta::presets::toggle_button(node.get_meta_group().current(),
                                         key,
                                         label,
                                         default_val);
  apply_category_if_set(node, a);
  return a;
}

meta::Attribute<bool> &add_bool(BaseNode          &node,
                                const std::string &key,
                                const std::string &label,
                                const std::string &label_true,
                                const std::string &label_false,
                                bool               default_val)
{
  auto &a = meta::presets::binary_buttons(node.get_meta_group().current(),
                                          key,
                                          label,
                                          label_true,
                                          label_false,
                                          default_val);
  apply_category_if_set(node, a);
  return a;
}

meta::Attribute<std::string> &add_choice(BaseNode                       &node,
                                         const std::string              &key,
                                         const std::string              &label,
                                         const std::vector<std::string> &choices,
                                         const std::string              &default_choice)
{
  auto &a = meta::presets::string_choice(node.get_meta_group().current(),
                                         key,
                                         label,
                                         choices,
                                         default_choice);
  apply_category_if_set(node, a);
  return a;
}

meta::Attribute<std::vector<glm::vec3>> &add_cloud(BaseNode          &node,
                                                   const std::string &key,
                                                   const std::string &label)
{
  auto &a = meta::presets::points(node.get_meta_group().current(), key, label);
  a.metadata().try_add(meta::keys::ui::widget_type, std::string("PointsEditor"));
  apply_category_if_set(node, a);
  return a;
}

meta::Attribute<glm::vec4> &add_color(BaseNode          &node,
                                      const std::string &key,
                                      const std::string &label,
                                      const glm::vec4   &default_color)
{
  auto &a = meta::presets::color(node.get_meta_group().current(),
                                 key,
                                 label,
                                 default_color);
  apply_category_if_set(node, a);
  return a;
}

meta::Attribute<meta::ColorGradient> &add_color_gradient(BaseNode          &node,
                                                         const std::string &key,
                                                         const std::string &label)
{
  auto &a = meta::presets::color_gradient(node.get_meta_group().current(), key, label);
  apply_category_if_set(node, a);
  return a;
}

meta::Attribute<std::vector<float>> &add_curve(BaseNode                 &node,
                                               const std::string        &key,
                                               const std::string        &label,
                                               const std::vector<float> &default_points,
                                               float                     vmin,
                                               float                     vmax)
{
  auto &a = meta::presets::curve(node.get_meta_group().current(),
                                 key,
                                 label,
                                 default_points,
                                 vmin,
                                 vmax);
  apply_category_if_set(node, a);
  return a;
}

meta::Attribute<int> &add_enum(BaseNode                                       &node,
                               const std::string                              &key,
                               const std::string                              &label,
                               const std::vector<std::pair<int, std::string>> &items,
                               int default_val)
{
  auto &a = meta::presets::enum_choice(node.get_meta_group().current(),
                                       key,
                                       label,
                                       items,
                                       default_val);
  apply_category_if_set(node, a);
  return a;
}

meta::Attribute<int> &add_enum(BaseNode                         &node,
                               const std::string                &key,
                               const std::string                &label,
                               const std::map<std::string, int> &enum_map,
                               const std::string                &default_choice)
{
  std::vector<std::pair<int, std::string>> items;
  for (const auto &[name, val] : enum_map)
    items.emplace_back(val, name);

  int default_val = enum_map.empty() ? 0 : enum_map.begin()->second;
  if (!default_choice.empty() && enum_map.contains(default_choice))
    default_val = enum_map.at(default_choice);

  return add_enum(node, key, label, items, default_val);
}

meta::Attribute<std::filesystem::path> &add_filename(BaseNode          &node,
                                                     const std::string &key,
                                                     const std::string &label,
                                                     const std::string &default_path,
                                                     const std::string &filter,
                                                     bool               is_save)
{
  auto &a = meta::presets::file(node.get_meta_group().current(),
                                key,
                                label,
                                default_path,
                                filter,
                                is_save);
  apply_category_if_set(node, a);
  return a;
}

meta::Attribute<float> &add_float(BaseNode          &node,
                                  const std::string &key,
                                  const std::string &label,
                                  float              default_val,
                                  float              vmin,
                                  float              vmax,
                                  const std::string &value_format,
                                  bool               log_scale)
{
  auto &a = meta::presets::slider_float(node.get_meta_group().current(),
                                        key,
                                        label,
                                        default_val,
                                        vmin,
                                        vmax,
                                        value_format,
                                        log_scale);
  apply_category_if_set(node, a);
  return a;
}

meta::Attribute<int> &add_int(BaseNode          &node,
                              const std::string &key,
                              const std::string &label,
                              int                default_val,
                              int                vmin,
                              int                vmax,
                              const std::string &value_format)
{
  auto &a = meta::presets::slider_int(node.get_meta_group().current(),
                                      key,
                                      label,
                                      default_val,
                                      vmin,
                                      vmax,
                                      value_format);
  apply_category_if_set(node, a);
  return a;
}

meta::Attribute<glm::vec2> &add_range(BaseNode          &node,
                                      const std::string &key,
                                      const std::string &label,
                                      const glm::vec2   &default_range,
                                      float              vmin,
                                      float              vmax,
                                      bool               is_active,
                                      const std::string &value_format)
{
  auto &a = meta::presets::range(node.get_meta_group().current(),
                                 key,
                                 label,
                                 default_range,
                                 vmin,
                                 vmax,
                                 is_active,
                                 value_format);
  apply_category_if_set(node, a);
  return a;
}

meta::Attribute<std::string> &add_read_only_text(BaseNode          &node,
                                                 const std::string &key,
                                                 const std::string &label,
                                                 const std::string &default_val)
{
  auto &a = add_string(node, key, label, default_val, /* read_only = */ true);
  a.metadata()
      .try_add(std::string(meta::keys::ui::widget_type), std::string("ReadOnlyText"))
      ->value() = "ReadOnlyText";
  return a;
}

meta::Attribute<int> &add_seed(BaseNode          &node,
                               const std::string &key,
                               const std::string &label,
                               unsigned int       default_val)
{
  auto &a = meta::presets::seed(node.get_meta_group().current(),
                                key,
                                label,
                                static_cast<int>(default_val));
  apply_category_if_set(node, a);
  return a;
}

meta::Attribute<std::string> &add_string(BaseNode          &node,
                                         const std::string &key,
                                         const std::string &label,
                                         const std::string &default_val,
                                         bool               multiline)
{
  auto &a = meta::presets::text(node.get_meta_group().current(),
                                key,
                                label,
                                default_val,
                                multiline);
  apply_category_if_set(node, a);
  return a;
}

meta::Attribute<glm::vec2> &add_wavenumber(BaseNode          &node,
                                           const std::string &key,
                                           const std::string &label,
                                           const glm::vec2   &default_val,
                                           float              vmin,
                                           float              vmax,
                                           bool               link_xy,
                                           const std::string &value_format)
{
  auto &a = meta::presets::wavenumber(node.get_meta_group().current(),
                                      key,
                                      label,
                                      default_val,
                                      vmin,
                                      vmax,
                                      link_xy,
                                      value_format);
  apply_category_if_set(node, a);
  return a;
}

meta::Attribute<glm::vec2> &add_xy(BaseNode          &node,
                                   const std::string &key,
                                   const std::string &label,
                                   const glm::vec2   &default_val,
                                   float              xmin,
                                   float              xmax,
                                   float              ymin,
                                   float              ymax)
{
  auto &a = meta::presets::xy(node.get_meta_group().current(),
                              key,
                              label,
                              default_val,
                              xmin,
                              xmax,
                              ymin,
                              ymax);
  apply_category_if_set(node, a);
  return a;
}

} // namespace hesiod
