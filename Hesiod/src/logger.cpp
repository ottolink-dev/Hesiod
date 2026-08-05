/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "hesiod/logger.hpp"

namespace hesiod
{

// Initialize the static member
std::shared_ptr<spdlog::logger> Logger::instance = nullptr;

std::shared_ptr<spdlog::logger> &Logger::log()
{
  if (!instance)
  {
    instance = spdlog::stdout_color_mt("console_hesiod");
    instance->set_pattern("[hesiod] [%H:%M:%S] [%^---%L---%$] %v");
    instance->set_level(spdlog::level::trace);

    // flush every record: redirected to a file the sink is block-buffered, so
    // an abnormal exit (abort, fast-fail) would otherwise discard the very
    // lines that explain it
    instance->flush_on(spdlog::level::trace);
  }
  return instance;
}

} // namespace hesiod
