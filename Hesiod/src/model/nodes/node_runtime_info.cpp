/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "hesiod/model/nodes/node_runtime_info.hpp"
#include "hesiod/model/utils.hpp"

namespace hesiod
{

int64_t helper_time_to_int64(const std::chrono::system_clock::time_point &tp)
{
  return std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch())
      .count();
}

void NodeRuntimeInfo::json_from(nlohmann::json const &json)
{
  int64_t ms = 0;

  json_safe_get<int64_t>(json, "time_creation", ms);
  time_creation = std::chrono::system_clock::time_point(std::chrono::milliseconds(ms));

  json_safe_get<int64_t>(json, "time_last_update", ms);
  time_last_update = std::chrono::system_clock::time_point(std::chrono::milliseconds(ms));

  json_safe_get(json, "update_time", update_time);
  json_safe_get(json, "eval_count", eval_count);
  json_safe_get(json, "error_count", error_count);
}

nlohmann::json NodeRuntimeInfo::json_to() const
{
  nlohmann::json json;

  json["time_creation"] = helper_time_to_int64(time_creation);
  json["time_last_update"] = helper_time_to_int64(time_last_update);
  json["update_time"] = this->update_time;
  json["eval_count"] = this->eval_count;
  json["error_count"] = this->error_count;

  return json;
}

} // namespace hesiod
