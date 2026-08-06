#pragma once
#include <nlohmann/json.hpp>
#include <string>

namespace hesiod
{
/**
 * @brief Converts a legacy serialized JSON attribute object into a standard
 *        Meta-compatible JSON attribute object based on its legacy type.
 * @param legacy_type The legacy type string (e.g., "Value range", "Cloud").
 * @param j The legacy attribute JSON object.
 * @return A Meta-compatible JSON representation of the attribute.
 */
nlohmann::json convert_legacy_attribute_json(const std::string &legacy_type,
                                             const nlohmann::json &j);
} // namespace hesiod
