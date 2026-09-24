/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "hesiod/model/error/error.hpp"

namespace hesiod
{

// =====================================
// ErrorManager
// =====================================
class ErrorManager
{
public:
  ErrorManager() = default;

  void push_error(const Error &error);
  void push_error(ErrorSeverity                      severity,
                  const std::string                 &category,
                  const std::string                 &message,
                  std::map<std::string, std::string> context = {});

  const std::vector<Error> &get_errors() const;
  std::vector<Error>        get_errors(ErrorSeverity severity) const;
  std::vector<Error>        get_errors_by_category(const std::string &category) const;
  std::vector<Error>        get_errors_for_context(const std::string &key,
                                                   const std::string &value) const;

  bool has_critical() const;
  bool has_errors() const;
  bool has_warnings() const;

  void clear();
  void clear_category(const std::string &category);

  // --- Callbacks
  std::function<void(const Error &)> error_occurred;

private:
  // --- Members
  std::vector<Error> errors;
};

} // namespace hesiod
