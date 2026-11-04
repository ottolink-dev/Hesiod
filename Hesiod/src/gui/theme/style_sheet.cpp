/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#include "hesiod/gui/theme/style_sheet.hpp"

namespace hesiod
{

QString render_stylesheet(std::string_view                          css_template,
                          const std::map<std::string, std::string> &string_tokens,
                          const std::map<std::string, QColor>      &color_tokens)
{
  QString result = QString::fromUtf8(css_template.data(),
                                     static_cast<int>(css_template.size()));

  for (const auto &[key, val] : string_tokens)
  {
    result.replace(QString::fromStdString(key), QString::fromStdString(val));
  }

  for (const auto &[key, color] : color_tokens)
  {
    result.replace(QString::fromStdString(key), color.name());
  }

  return result;
}

} // namespace hesiod
