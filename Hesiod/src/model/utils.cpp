/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <chrono>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>
#include <unordered_map>

#include "nlohmann/json.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/logger.hpp"

namespace fs = std::filesystem;

namespace hesiod
{

std::string ascii_progress_bar(float fraction,
                               int   width,
                               bool  show_percentage,
                               char  fill,
                               char  head,
                               char  empty)
{
  if (width < 4)
    width = 4; // minimum sensible width

  fraction = std::clamp(fraction, 0.f, 1.f);

  // number of filled characters (floor)
  int filled = static_cast<int>(std::floor(fraction * width));

  // Build the bar
  std::ostringstream ss;
  ss << '[';

  // filled part
  for (int i = 0; i < filled; ++i)
    ss << fill;

  // head (if not fully filled and we have space for it)
  if (filled < width)
  {
    // show head only if there's at least one empty slot
    ss << head;
    // remaining empties
    for (int i = filled + 1; i < width; ++i)
      ss << empty;
  }

  ss << ']';

  if (show_percentage)
  {
    int percent = static_cast<int>(std::round(fraction * 100.0));
    ss << ' ' << percent << '%';
  }

  return ss.str();
}

std::filesystem::path ensure_extension(std::filesystem::path fname,
                                       const std::string    &extension)
{
  std::string ext = extension;

  // ensure extension starts with a dot
  if (!ext.empty() && ext.front() != '.')
    ext = "." + ext;

  // if the extension is missing or different, replace it
  if (!fname.has_extension() || fname.extension() != ext)
    fname.replace_extension(ext);

  return fname;
}

std::filesystem::path insert_before_basename(const std::filesystem::path &original_path,
                                             const std::string           &insert_str)
{
  std::filesystem::path dir = original_path.parent_path();
  std::string           stem = original_path.stem().string();
  std::string           ext = original_path.extension().string();
  std::filesystem::path new_name = insert_str + stem + ext;
  return dir / new_name;
}

std::filesystem::path insert_before_extension(const std::filesystem::path &original_path,
                                              const std::string           &insert_str)
{
  std::filesystem::path dir = original_path.parent_path();
  std::string           stem = original_path.stem().string();
  std::string           ext = original_path.extension().string();
  std::filesystem::path new_name = stem + insert_str + ext;
  return dir / new_name;
}

std::string insert_char_every_nth(const std::string &input,
                                  std::size_t        n,
                                  const std::string &chr)
{
  if (n == 0)
    return input; // avoid division by zero

  std::string result;
  result.reserve(input.size() + input.size() / n * chr.size());

  for (std::size_t i = 0; i < input.size(); ++i)
  {
    result += input[i];
    if ((i + 1) % n == 0)
    {
      result += chr;
    }
  }

  return result;
}

#include <filesystem>
#include <regex>
#include <string>
#include <unordered_map>

namespace fs = std::filesystem;

std::string int_to_string(int value, int width = 0)
{
  std::string result = std::to_string(value);

  if (width > 0 && result.size() < static_cast<std::size_t>(width))
    result.insert(0, width - result.size(), '0');

  return result;
}

nlohmann::json json_from_file(const std::string &fname)
{
  nlohmann::json json;
  std::ifstream  file(fname);

  if (file.is_open())
  {
    file >> json;
    file.close();
    Logger::log()->trace("json_from_file: JSON successfully loaded from {}", fname);
  }
  else
  {
    Logger::log()->error("json_from_file: Could not open file {} to load JSON", fname);
  }

  return json;
}

void json_to_file(const nlohmann::json &json,
                  const std::string    &fname,
                  bool                  merge_with_existing_content)
{
  nlohmann::json final_json = json;

  if (merge_with_existing_content)
  {
    std::ifstream infile(fname);
    if (infile.is_open())
    {
      try
      {
        nlohmann::json existing;
        infile >> existing;
        infile.close();

        // Merge new JSON into existing JSON
        existing.merge_patch(json);
        final_json = existing;

        Logger::log()->trace("json_to_file: merged JSON with existing content in {}",
                             fname);
      }
      catch (const std::exception &e)
      {
        Logger::log()->warn(
            "json_to_file: Could not parse existing JSON in {} ({}). Overwriting "
            "instead.",
            fname,
            e.what());
      }
    }
  }

  std::ofstream outfile(fname);
  if (outfile.is_open())
  {
    outfile << final_json.dump(4);
    outfile.close();
    Logger::log()->trace("json_to_file: JSON successfully written to {}", fname);
  }
  else
  {
    Logger::log()->error("json_to_file: Could not open file {} to save JSON", fname);
  }
}

fs::path make_unique_filename(
    const fs::path                                     &output_directory,
    std::string                                         filename,
    const std::unordered_map<std::string, std::string> &replacements)
{
  // Apply user-defined replacements.
  for (const auto &[placeholder, value] : replacements)
  {
    std::size_t pos = 0;

    while ((pos = filename.find(placeholder, pos)) != std::string::npos)
    {
      filename.replace(pos, placeholder.size(), value);
      pos += value.size();
    }
  }

  // Find {INC} or {INC:N}.
  const std::regex inc_regex(R"(\{INC(?::([0-9]+))?\})");
  std::smatch      match;

  if (!std::regex_search(filename, match, inc_regex))
  {
    const fs::path output_path = output_directory / filename;

    if (fs::exists(output_path))
      Logger::log()->warn("Output file already exists: {}. "
                          "Filename pattern does not contain INC placeholder.",
                          output_path.string());

    return output_path;
  }

  const int width = match[1].matched ? std::stoi(match[1].str()) : 0;

  const std::string inc_placeholder = match[0].str();

  for (int inc = 0;; ++inc)
  {
    const std::string inc_string = int_to_string(inc, width);

    std::string candidate = filename;

    std::size_t pos = 0;
    while ((pos = candidate.find(inc_placeholder, pos)) != std::string::npos)
    {
      candidate.replace(pos, inc_placeholder.size(), inc_string);
      pos += inc_string.size();
    }

    const fs::path output_path = output_directory / candidate;

    if (!fs::exists(output_path))
      return output_path;
  }
}

std::filesystem::path prepend_project_name_to_path(
    const std::filesystem::path &original_path)
{
  // preprend project name if any and if requested
  std::string project_name = HSD_CTX.project_model->get_name();

  if (project_name.empty())
    return original_path;

  return insert_before_basename(original_path, project_name + "_");
}

std::string ptr_as_string(void *ptr)
{
  if (!ptr)
    return "0x0";

  std::ostringstream oss;
  oss << ptr;
  return oss.str();
}

std::string remove_trailing_char(const std::string &input, char ch)
{
  size_t end = input.size();

  while (end > 0 && input[end - 1] == ch)
    --end;

  return input.substr(0, end);
}

void replace_all(std::string &str, const std::string &from, const std::string &to)
{
  if (from.empty())
    return; // avoid infinite loop

  size_t start_pos = 0;
  while ((start_pos = str.find(from, start_pos)) != std::string::npos)
  {
    str.replace(start_pos, from.length(), to);
    start_pos += to.length(); // advance past the replacement
  }
}

std::vector<std::string> split_string(const std::string &string, char delimiter)
{
  std::vector<std::string> result;
  std::stringstream        ss(string);
  std::string              word;

  while (std::getline(ss, word, delimiter))
    result.push_back(word);

  return result;
}

void string_to_file(const std::string &content, const std::string &fname)
{
  std::ofstream file(fname);

  if (!file.is_open())
  {
    throw std::runtime_error("Failed to open file: " + fname);
  }

  file << content;
}

std::string timestamp(std::chrono::system_clock::time_point tp)
{
  std::time_t t = std::chrono::system_clock::to_time_t(tp);

  std::tm buf{};
#if defined(_WIN32)
  localtime_s(&buf, &t); // Windows
#else
  localtime_r(&t, &buf); // Linux / POSIX
#endif

  std::stringstream ss;
  ss << std::put_time(&buf, "%Y-%m-%d_%H-%M-%S");
  return ss.str();
}

std::string timestamp() { return timestamp(std::chrono::system_clock::now()); }

unsigned int to_uint_safe(const std::string &str)
{
  try
  {
    size_t        pos;
    unsigned long val = std::stoul(str, &pos);
    if (pos != str.size())
    {
      throw std::invalid_argument("Trailing characters");
    }
    if (val > std::numeric_limits<unsigned int>::max())
    {
      throw std::out_of_range("Value too large for unsigned int");
    }
    return static_cast<unsigned int>(val);
  }
  catch (const std::exception &e)
  {
    Logger::log()->error("Conversion error: {}", e.what());
    return 0; // or throw / handle differently
  }
}

std::string wrap_text(const std::string &text, std::size_t max_len)
{
  std::ostringstream out;
  std::istringstream iss(text);

  std::string word;
  std::string current;

  while (iss >> word)
  {
    if (current.empty())
    {
      current = word;
    }
    else if (current.size() + 1 + word.size() <= max_len)
    {
      current += " " + word;
    }
    else
    {
      out << current << "\n"; // write completed line
      current = word;
    }
  }

  if (!current.empty())
    out << current;

  return out.str();
}

} // namespace hesiod
