/* Copyright (c) 2024 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"

#include "meta/core/attribute.hpp"
#include "meta/ext/color_gradient/color_gradient.hpp"

namespace hesiod
{

class BaseNode; // forward declaration

// -----------------------------------------------------------------------------
// Pure Attribute Builders (Alphabetical Order in namespace hesiod)
// -----------------------------------------------------------------------------

meta::Attribute<float> &add_angle(BaseNode          &node,
                                  const std::string &key = "angle",
                                  const std::string &label = "Angle",
                                  float              default_val = 0.f,
                                  float              vmin = -180.f,
                                  float              vmax = 180.f,
                                  const std::string &value_format = "{:.1f}°");

meta::Attribute<bool> &add_bool(BaseNode          &node,
                                const std::string &key,
                                const std::string &label,
                                bool               default_val = false);

meta::Attribute<bool> &add_bool(BaseNode          &node,
                                const std::string &key,
                                const std::string &label,
                                const std::string &label_true,
                                const std::string &label_false,
                                bool               default_val = false);

meta::Attribute<std::string> &add_choice(BaseNode                       &node,
                                         const std::string              &key,
                                         const std::string              &label,
                                         const std::vector<std::string> &choices,
                                         const std::string &default_choice = "");

meta::Attribute<std::vector<glm::vec3>> &add_cloud(BaseNode          &node,
                                                   const std::string &key,
                                                   const std::string &label = "Cloud");

meta::Attribute<glm::vec4> &add_color(BaseNode          &node,
                                      const std::string &key,
                                      const std::string &label,
                                      const glm::vec4   &default_color);

meta::Attribute<meta::ColorGradient> &add_color_gradient(BaseNode          &node,
                                                         const std::string &key,
                                                         const std::string &label);

meta::Attribute<std::vector<float>> &add_curve(
    BaseNode                 &node,
    const std::string        &key,
    const std::string        &label,
    const std::vector<float> &default_points = {},
    float                     vmin = 0.f,
    float                     vmax = 1.f);

meta::Attribute<int> &add_enum(BaseNode                                       &node,
                               const std::string                              &key,
                               const std::string                              &label,
                               const std::vector<std::pair<int, std::string>> &items,
                               int default_val);

meta::Attribute<int> &add_enum(BaseNode                         &node,
                               const std::string                &key,
                               const std::string                &label,
                               const std::map<std::string, int> &enum_map,
                               const std::string                &default_choice = "");

meta::Attribute<std::filesystem::path> &add_filename(
    BaseNode          &node,
    const std::string &key,
    const std::string &label,
    const std::string &default_path = "",
    const std::string &filter = "All Files (*.*)",
    bool               is_save = false);

meta::Attribute<float> &add_float(BaseNode          &node,
                                  const std::string &key,
                                  const std::string &label,
                                  float              default_val,
                                  float              vmin,
                                  float              vmax,
                                  const std::string &value_format = "{:.2f}",
                                  bool               log_scale = false);

meta::Attribute<int> &add_int(BaseNode          &node,
                              const std::string &key,
                              const std::string &label,
                              int                default_val,
                              int                vmin,
                              int                vmax,
                              const std::string &value_format = "{}");

meta::Attribute<glm::vec2> &add_range(BaseNode          &node,
                                      const std::string &key,
                                      const std::string &label,
                                      const glm::vec2   &default_range,
                                      float              vmin,
                                      float              vmax,
                                      bool               is_active = true,
                                      const std::string &value_format = "{:.3f}");

meta::Attribute<std::string> &add_read_only_text(BaseNode          &node,
                                                 const std::string &key,
                                                 const std::string &label,
                                                 const std::string &default_val = "");

meta::Attribute<int> &add_seed(BaseNode          &node,
                               const std::string &key,
                               const std::string &label = "Seed",
                               unsigned int       default_val = 0);

meta::Attribute<std::string> &add_string(BaseNode          &node,
                                         const std::string &key,
                                         const std::string &label,
                                         const std::string &default_val = "",
                                         bool               multiline = false);

meta::Attribute<glm::vec2> &add_wavenumber(BaseNode          &node,
                                           const std::string &key,
                                           const std::string &label = "Spatial Frequency",
                                           const glm::vec2   &default_val = {2.f, 2.f},
                                           float              vmin = 0.f,
                                           float              vmax = 64.f,
                                           bool               link_xy = true,
                                           const std::string &value_format = "{:.2f}");

meta::Attribute<glm::vec2> &add_xy(BaseNode          &node,
                                    const std::string &key,
                                    const std::string &label,
                                    const glm::vec2   &default_val = {0.5f, 0.5f},
                                    float              xmin = 0.f,
                                    float              xmax = 1.f,
                                    float              ymin = 0.f,
                                    float              ymax = 1.f);

} // namespace hesiod
