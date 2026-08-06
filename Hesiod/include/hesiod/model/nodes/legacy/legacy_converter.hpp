#pragma once
#include <nlohmann/json.hpp>

namespace meta
{
class AbstractAttribute;
}

namespace hesiod
{
/**
 * @brief Converts a legacy serialized JSON attribute object into a standard
 *        Meta-compatible JSON attribute object based on its C++ attribute type.
 * @param attr The attribute instance.
 * @param j The legacy attribute JSON object.
 * @return A Meta-compatible JSON representation of the attribute.
 */
nlohmann::json convert_legacy_attribute_json(const meta::AbstractAttribute *attr,
                                             const nlohmann::json          &j);
} // namespace hesiod
