/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>

#include "hesiod/logger.hpp"
#include "hesiod/model/error/error_manager.hpp"

namespace hesiod
{

void ErrorManager::clear() { this->errors.clear(); }

void ErrorManager::clear_category(ErrorCategory category)
{
  std::erase_if(this->errors,
                [category](const Error &err) { return err.get_category() == category; });
}

const std::vector<Error> &ErrorManager::get_errors() const { return this->errors; }

std::vector<Error> ErrorManager::get_errors_by_category(ErrorCategory category) const
{
  std::vector<Error> result;
  for (const auto &err : this->errors)
  {
    if (err.get_category() == category)
      result.push_back(err);
  }
  return result;
}

bool ErrorManager::has_errors() const { return !this->errors.empty(); }

void ErrorManager::push_error(const Error &error)
{
  Logger::log()->trace("ErrorManager::push_error: {}", error.get_message());

  this->errors.push_back(error);

  if (this->error_occurred)
    this->error_occurred(error);
}

void ErrorManager::push_error(ErrorCategory category, const std::string &message)
{
  Error err(category, message);
  this->push_error(err);
}

} // namespace hesiod
