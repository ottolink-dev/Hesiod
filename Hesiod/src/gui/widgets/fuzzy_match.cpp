/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <climits>
#include <vector>

#include <QRegularExpression>
#include <QStringList>

#include "hesiod/gui/widgets/fuzzy_match.hpp"

namespace hesiod
{

namespace
{

// optimal string alignment distance: edits, with adjacent swaps counting as one
int edit_distance(const QString &a, const QString &b)
{
  const int                     n = int(a.size());
  const int                     m = int(b.size());
  std::vector<std::vector<int>> d(n + 1, std::vector<int>(m + 1));
  for (int i = 0; i <= n; ++i)
    d[i][0] = i;
  for (int j = 0; j <= m; ++j)
    d[0][j] = j;

  for (int i = 1; i <= n; ++i)
    for (int j = 1; j <= m; ++j)
    {
      const int cost = a[i - 1] == b[j - 1] ? 0 : 1;
      d[i][j] = std::min({d[i - 1][j] + 1, d[i][j - 1] + 1, d[i - 1][j - 1] + cost});
      if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1])
        d[i][j] = std::min(d[i][j], d[i - 2][j - 2] + 1);
    }
  return d[n][m];
}

bool word_start(const QString &text, qsizetype i)
{
  return i == 0 || !text[i - 1].isLetterOrNumber();
}
} // namespace

// Score of one query token against one (lower-cased) field; 0 = no match.
// Substrings win, then abbreviations ("ui scl" -> "interface scale" does not,
// but "intsc" does), then typos ("scael", "animtions").
int fuzzy_token_score(const QString &token, const QString &text, bool allow_subsequence)
{
  if (token.isEmpty() || text.isEmpty())
    return 0;

  // substring, best at a word start and early in the text
  const qsizetype at = text.indexOf(token);
  if (at >= 0)
    return 100 + (word_start(text, at) ? 40 : 0) - int(std::min<qsizetype>(at, 30));

  // abbreviation: every character in order, rewarding word starts and runs
  if (allow_subsequence && token.size() >= 2)
  {
    int       score = 0, streak = 0;
    qsizetype ti = 0, first = -1, last = -1;
    for (qsizetype i = 0; i < text.size() && ti < token.size(); ++i)
    {
      if (text[i] == token[ti])
      {
        if (first < 0)
          first = i;
        last = i;
        score += 2 + (word_start(text, i) ? 6 : 0) + 3 * streak;
        ++streak;
        ++ti;
      }
      else
        streak = 0;
    }

    // all found, and not scattered over the whole field
    if (ti == token.size() && (last - first) <= 4 * token.size())
      return std::clamp(score, 10, 90);
  }

  // typo tolerance against each word (and each word's prefix, for words the
  // user has not finished typing)
  if (token.size() >= 4)
  {
    static const QRegularExpression separators("[^\\p{L}\\p{N}]+");
    int                             best = INT_MAX;
    for (const QString &word : text.split(separators, Qt::SkipEmptyParts))
    {
      best = std::min(best, edit_distance(token, word));
      if (word.size() > token.size())
        best = std::min(best, edit_distance(token, word.left(token.size())));
    }

    const int allowed = token.size() >= 7 ? 2 : 1;
    if (best <= allowed)
      return 60 - 20 * best;
  }

  return 0;
}

} // namespace hesiod
