/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <functional>
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
  void push_error(ErrorCategory category, const std::string &message);

  const std::vector<Error> &get_errors() const;
  std::vector<Error>        get_errors_by_category(ErrorCategory category) const;

  bool has_errors() const;

  void clear();
  void clear_category(ErrorCategory category);

  // --- Callbacks
  std::function<void(const Error &)> error_occurred;

private:
  // --- Members
  std::vector<Error> errors;
};

} // namespace hesiod
