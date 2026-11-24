/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QString>

namespace hesiod
{

// Score of one search token against one lower-cased text field, 0 for no
// match. Substrings score highest (more at a word start and early in the
// text), then abbreviations ("intsc" for "interface scale", when
// allow_subsequence), then typos ("scael", "animtions").
int fuzzy_token_score(const QString &token, const QString &text, bool allow_subsequence);

} // namespace hesiod
