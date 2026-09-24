/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <chrono>
#include <string>

namespace hesiod
{

enum class ErrorCategory
{
  General,
  IO,
  Deserialization,
  NodeCreation,
  LinkCreation,
  GraphExecution,
  Bake,
  Export
};

std::string error_category_to_string(ErrorCategory category);

// =====================================
// Error
// =====================================
class Error
{
public:
  Error() = default;
  Error(ErrorCategory category, std::string message);

  // --- Fluent builder methods
  Error &with_category(ErrorCategory new_category);

  // --- Accessors
  ErrorCategory                         get_category() const;
  std::string                           get_category_string() const;
  const std::string                    &get_message() const;
  std::chrono::system_clock::time_point get_timestamp() const;

private:
  // --- Members
  ErrorCategory                         category = ErrorCategory::General;
  std::string                           message;
  std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now();
};

} // namespace hesiod
