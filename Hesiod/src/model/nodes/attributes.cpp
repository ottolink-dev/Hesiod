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

static void set_doc_type(meta::AbstractAttribute &a, const char *type_name)
{
  a.metadata().try_add(std::string(ATTR_DOC_TYPE_KEY), std::string(type_name));
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

meta::Attribute<meta::Array> &add_array(BaseNode          &node,
                                        const std::string &key,
                                        const std::string &label)
{
  auto      &c = node.get_meta_group().current();
  const auto shape = node.cfg().shape;

  auto *a = c.add<meta::Array>(key,
                               meta::Array{shape, std::vector(shape.x * shape.y, 0.f)});

  a->metadata().try_add(meta::keys::ui::label, std::string(label));
  a->metadata().try_add(meta::keys::ui::width, shape.x);
  a->metadata().try_add(meta::keys::ui::height, shape.y);

  set_doc_type(*a, "Array");
  apply_category_if_set(node, *a);

  return *a;
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
  set_doc_type(a, "Bool");
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
  set_doc_type(a, "Bool");
  apply_category_if_set(node, a);
  return a;
}

meta::Attribute<std::string> &add_choice(BaseNode                       &node,
                                         const std::string              &key,
                                         const std::string              &label,
                                         const std::vector<std::string> &choices,
                                         const std::string              &default_choice)
{
  std::string value = default_choice;
  if (value.empty() && !choices.empty())
    value = choices.front();

  auto &a = meta::presets::string_choice(node.get_meta_group().current(),
                                         key,
                                         label,
                                         choices,
                                         value);
  set_doc_type(a, "Choice");
  apply_category_if_set(node, a);
  return a;
}

meta::Attribute<std::vector<glm::vec3>> &add_cloud(BaseNode          &node,
                                                   const std::string &key,
                                                   const std::string &label)
{
  auto &a = meta::presets::points(node.get_meta_group().current(), key, label);
  a.metadata().try_add(meta::keys::ui::widget_type, std::string("PointsEditor"));
  set_doc_type(a, "Cloud");
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
  set_doc_type(a, "Color");
  apply_category_if_set(node, a);
  return a;
}

meta::Attribute<meta::ColorGradient> &add_color_gradient(BaseNode          &node,
                                                         const std::string &key,
                                                         const std::string &label)
{
  auto &a = meta::presets::color_gradient(node.get_meta_group().current(), key, label);
  a.metadata().try_add(meta::keys::ui::presets, meta::GradientPresets{});
  set_doc_type(a, "Color gradient");
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
  set_doc_type(a, "Vector of floats");
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
  set_doc_type(a, "Enumeration");
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

meta::Attribute<std::filesystem::path> &add_filename(
    BaseNode                    &node,
    const std::string           &key,
    const std::string           &label,
    const std::filesystem::path &default_path,
    const std::string           &filter,
    bool                         is_save)
{
  auto &a = meta::presets::file(node.get_meta_group().current(),
                                key,
                                label,
                                default_path,
                                filter,
                                is_save);
  set_doc_type(a, "Filename");
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
  set_doc_type(a, "Float");
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
  set_doc_type(a, "Integer");
  apply_category_if_set(node, a);
  return a;
}

meta::Attribute<std::vector<glm::vec3>> &add_path(BaseNode          &node,
                                                  const std::string &key,
                                                  const std::string &label,
                                                  bool               closed)
{
  auto &a = meta::presets::points(node.get_meta_group().current(), key, label);
  a.metadata().try_add(meta::keys::ui::widget_type, std::string("PathEditor"));
  a.metadata().try_add(std::string(meta::keys::ui::closed), closed);
  set_doc_type(a, "Path");
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
  set_doc_type(a, "Value range");
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
  set_doc_type(a, "Random seed number");
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
  set_doc_type(a, "String");
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
  set_doc_type(a, "Wavenumber");
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
  set_doc_type(a, "Vec2Float");
  apply_category_if_set(node, a);
  return a;
}

} // namespace hesiod
