/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <chrono>
#include <format>
#include <map>
#include <string>
#include <utility>

namespace hesiod
{

enum class ErrorSeverity
{
  Info,
  Warning,
  Error,
  Critical
};

// =====================================
// Error
// =====================================
class Error
{
public:
  Error() = default;

  Error(ErrorSeverity severity,
        std::string   category,
        std::string   message,
        std::string   source = "")
      : severity(severity), category(std::move(category)), message(std::move(message)),
        source(std::move(source)), timestamp(std::chrono::system_clock::now())
  {
  }

  // --- Static factory helpers
  static Error info(std::string message, std::string category = "General")
  {
    return Error(ErrorSeverity::Info, std::move(category), std::move(message));
  }

  static Error warning(std::string message, std::string category = "General")
  {
    return Error(ErrorSeverity::Warning, std::move(category), std::move(message));
  }

  static Error error(std::string message, std::string category = "General")
  {
    return Error(ErrorSeverity::Error, std::move(category), std::move(message));
  }

  static Error critical(std::string message, std::string category = "General")
  {
    return Error(ErrorSeverity::Critical, std::move(category), std::move(message));
  }

  // --- Fluent builder methods
  Error &with_category(std::string new_category)
  {
    this->category = std::move(new_category);
    return *this;
  }

  Error &with_code(std::string new_code)
  {
    this->code = std::move(new_code);
    return *this;
  }

  Error &with_context(std::string key, std::string value)
  {
    if (!value.empty())
      this->context[std::move(key)] = std::move(value);
    return *this;
  }

  Error &with_context(std::map<std::string, std::string> ctx)
  {
    for (auto &[k, v] : ctx)
    {
      if (!v.empty())
        this->context[std::move(k)] = std::move(v);
    }
    return *this;
  }

  Error &with_source(std::string new_source)
  {
    this->source = std::move(new_source);
    return *this;
  }

  // --- Accessors
  const std::string &get_category() const { return this->category; }
  const std::string &get_code() const { return this->code; }
  const std::map<std::string, std::string> &get_context() const { return this->context; }
  const std::string                        &get_message() const { return this->message; }
  ErrorSeverity                         get_severity() const { return this->severity; }
  const std::string                    &get_source() const { return this->source; }
  std::chrono::system_clock::time_point get_timestamp() const { return this->timestamp; }

  std::string get_context_value(const std::string &key) const
  {
    auto it = this->context.find(key);
    return it != this->context.end() ? it->second : "";
  }

  bool has_context(const std::string &key) const { return this->context.contains(key); }

  std::string formatted_message() const
  {
    std::string prefix;
    if (auto it = this->context.find("graph_id");
        it != this->context.end() && !it->second.empty())
      prefix += std::format("Graph '{}': ", it->second);
    if (auto it = this->context.find("node_id");
        it != this->context.end() && !it->second.empty())
      prefix += std::format("Node '{}': ", it->second);
    if (auto it = this->context.find("port_id");
        it != this->context.end() && !it->second.empty())
      prefix += std::format("Port '{}': ", it->second);
    if (auto it = this->context.find("file_path");
        it != this->context.end() && !it->second.empty())
      prefix += std::format("File '{}': ", it->second);

    return prefix.empty() ? this->message : (prefix + this->message);
  }

private:
  // --- Members
  ErrorSeverity                         severity = ErrorSeverity::Error;
  std::string                           category = "General";
  std::string                           message;
  std::string                           code;
  std::string                           source;
  std::map<std::string, std::string>    context;
  std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now();
};

} // namespace hesiod
