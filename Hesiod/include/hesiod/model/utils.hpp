/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <sstream>
#include <string>
#include <unordered_set>

#include <QColor>

#include "nlohmann/json.hpp"
#include "nlohmann/json_fwd.hpp"

#include "hesiod/logger.hpp"

namespace hesiod
{

// --- Vectors

template <typename T> bool contains(const std::vector<T> &vec, const T &item)
{
  return std::find(vec.begin(), vec.end(), item) != vec.end();
}

template <typename T, typename U>
int find_index(const std::vector<T> &vec, const U &value)
{
  auto it = std::find(vec.begin(), vec.end(), value);
  return (it != vec.end()) ? std::distance(vec.begin(), it) : -1;
}

template <typename T>
std::vector<T> merge_unique(const std::vector<T> &a, const std::vector<T> &b)
{
  std::unordered_set<T> seen;
  std::vector<T>        result;

  for (const auto &item : a)
  {
    if (seen.insert(item).second)
      result.push_back(item);
  }

  for (const auto &item : b)
  {
    if (seen.insert(item).second)
      result.push_back(item);
  }

  return result;
}

template <typename T> void remove_all_occurrences(std::vector<T> &vec, const T &value)
{
  vec.erase(std::remove(vec.begin(), vec.end(), value), vec.end());
}

// --- Filenames

std::filesystem::path ensure_extension(std::filesystem::path fname,
                                       const std::string    &extension);

std::filesystem::path insert_before_basename(const std::filesystem::path &original_path,
                                             const std::string           &insert_str);

std::filesystem::path insert_before_extension(const std::filesystem::path &original_path,
                                              const std::string           &insert_str);

std::filesystem::path make_unique_filename(
    const std::filesystem::path                        &output_directory,
    std::string                                         filename,
    const std::unordered_map<std::string, std::string> &replacements);

std::filesystem::path prepend_project_name_to_path(
    const std::filesystem::path &original_path);

// --- json

nlohmann::json json_from_file(const std::string &fname);
void           json_to_file(const nlohmann::json &json,
                            const std::string    &fname,
                            bool                  merge_with_existing_content = false);

template <typename T>
inline void json_safe_get(const nlohmann::json &j, const std::string &key, T &value)
{
  if (j.contains(key))
  {
    value = j.at(key).get<T>();
  }
  else
  {
    Logger::log()->error("Required json key \"{}\" not found.", key);
  }
}

inline void json_safe_get(const nlohmann::json &j, const std::string &key, QColor &value)
{
  if (j.contains(key))
  {
    QString str = j.at(key).get<std::string>().c_str();
    if (QColor::isValidColor(str))
      value = QColor(str);
    else
      Logger::log()->error("Invalid QColor for key \"{}\"", key);
  }
  else
  {
    Logger::log()->error("Required json key \"{}\" not found.", key);
  }
}

// --- Strings

std::string insert_char_every_nth(const std::string &input,
                                  std::size_t        n,
                                  const std::string &chr);

// int_to_string(42);       // "42"
// int_to_string(42, 4);    // "0042"
std::string int_to_string(int value, int width = 0);

std::string ptr_as_string(void *ptr);
std::string remove_trailing_char(const std::string &input, char ch);
void        replace_all(std::string &str, const std::string &from, const std::string &to);
std::vector<std::string> split_string(const std::string &string, char delimiter);
void         string_to_file(const std::string &content, const std::string &fname);
std::string  timestamp(std::chrono::system_clock::time_point tp);
std::string  timestamp();
unsigned int to_uint_safe(const std::string &str);
std::string  wrap_text(const std::string &text, std::size_t max_len);

std::string ascii_progress_bar(float fraction,
                               int   width = 40,
                               bool  show_percentage = true,
                               char  fill = '=',
                               char  head = '>',
                               char  empty = ' ');

} // namespace hesiod
