/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <map>
#include <string>
#include <string_view>

#include <QColor>
#include <QString>

namespace hesiod
{

// Render a stylesheet template by replacing tokens like "TOKEN" or "{{TOKEN}}"
// with corresponding color/string values.
QString render_stylesheet(std::string_view                          css_template,
                          const std::map<std::string, std::string> &string_tokens,
                          const std::map<std::string, QColor>      &color_tokens = {});

} // namespace hesiod
