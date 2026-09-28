/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "hesiod/model/error/error.hpp"

#include <utility>

namespace hesiod
{

std::string error_category_to_string(ErrorCategory category)
{
  switch (category)
  {
  case ErrorCategory::General:
    return "General";
  case ErrorCategory::IO:
    return "IO";
  case ErrorCategory::Deserialization:
    return "Deserialization";
  case ErrorCategory::NodeCreation:
    return "NodeCreation";
  case ErrorCategory::LinkCreation:
    return "LinkCreation";
  case ErrorCategory::GraphExecution:
    return "GraphExecution";
  case ErrorCategory::Bake:
    return "Bake";
  case ErrorCategory::Export:
    return "Export";
  }
  return "Unknown";
}

Error::Error(ErrorCategory category, std::string message)
    : category(category), message(std::move(message)),
      timestamp(std::chrono::system_clock::now())
{
}

Error &Error::with_category(ErrorCategory new_category)
{
  this->category = new_category;
  return *this;
}

ErrorCategory Error::get_category() const { return this->category; }

std::string Error::get_category_string() const
{
  return error_category_to_string(this->category);
}

const std::string &Error::get_message() const { return this->message; }

std::chrono::system_clock::time_point Error::get_timestamp() const
{
  return this->timestamp;
}

} // namespace hesiod
