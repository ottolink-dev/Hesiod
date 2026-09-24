/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>

#include "hesiod/logger.hpp"
#include "hesiod/model/error/error_manager.hpp"

namespace hesiod
{

void ErrorManager::clear() { this->errors.clear(); }

void ErrorManager::clear_category(const std::string &category)
{
  std::erase_if(this->errors,
                [&category](const Error &err) { return err.get_category() == category; });
}

const std::vector<Error> &ErrorManager::get_errors() const { return this->errors; }

std::vector<Error> ErrorManager::get_errors(ErrorSeverity severity) const
{
  std::vector<Error> result;
  for (const auto &err : this->errors)
  {
    if (err.get_severity() == severity)
      result.push_back(err);
  }
  return result;
}

std::vector<Error> ErrorManager::get_errors_by_category(const std::string &category) const
{
  std::vector<Error> result;
  for (const auto &err : this->errors)
  {
    if (err.get_category() == category)
      result.push_back(err);
  }
  return result;
}

std::vector<Error> ErrorManager::get_errors_for_context(const std::string &key,
                                                        const std::string &value) const
{
  std::vector<Error> result;
  for (const auto &err : this->errors)
  {
    if (err.get_context_value(key) == value)
      result.push_back(err);
  }
  return result;
}

bool ErrorManager::has_critical() const
{
  return std::any_of(this->errors.begin(),
                     this->errors.end(),
                     [](const Error &err)
                     { return err.get_severity() == ErrorSeverity::Critical; });
}

bool ErrorManager::has_errors() const { return !this->errors.empty(); }

bool ErrorManager::has_warnings() const
{
  return std::any_of(this->errors.begin(),
                     this->errors.end(),
                     [](const Error &err)
                     { return err.get_severity() == ErrorSeverity::Warning; });
}

void ErrorManager::push_error(const Error &error)
{
  const std::string msg = error.formatted_message();

  switch (error.get_severity())
  {
  case ErrorSeverity::Info:
    Logger::log()->info("{}", msg);
    break;
  case ErrorSeverity::Warning:
    Logger::log()->warn("{}", msg);
    break;
  case ErrorSeverity::Error:
    Logger::log()->error("{}", msg);
    break;
  case ErrorSeverity::Critical:
    Logger::log()->critical("{}", msg);
    break;
  }

  this->errors.push_back(error);

  if (this->error_occurred)
    this->error_occurred(error);
}

void ErrorManager::push_error(ErrorSeverity                      severity,
                              const std::string                 &category,
                              const std::string                 &message,
                              std::map<std::string, std::string> context)
{
  Error err(severity, category, message);
  err.with_context(std::move(context));
  this->push_error(err);
}

} // namespace hesiod
