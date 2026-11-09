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

  std::vector<std::pair<QString, QString>> replacements;
  replacements.reserve(string_tokens.size() + color_tokens.size());

  for (const auto &[key, val] : string_tokens)
  {
    replacements.emplace_back(QString::fromStdString(key), QString::fromStdString(val));
  }

  for (const auto &[key, color] : color_tokens)
  {
    replacements.emplace_back(QString::fromStdString(key), color.name());
  }

  // Sort tokens by length descending so longer tokens (e.g. "DANGER_SOFT")
  // are replaced before their prefixes (e.g. "DANGER").
  std::sort(replacements.begin(),
            replacements.end(),
            [](const auto &a, const auto &b)
            {
              if (a.first.length() != b.first.length())
                return a.first.length() > b.first.length();
              return a.first < b.first;
            });

  for (const auto &[from, to] : replacements)
  {
    result.replace(from, to);
  }

  return result;
}

} // namespace hesiod
