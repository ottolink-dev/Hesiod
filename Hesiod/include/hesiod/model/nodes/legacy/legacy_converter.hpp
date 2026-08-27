#pragma once
#include <nlohmann/json.hpp>

namespace meta
{
class AbstractAttribute;
class ContainerGroup;
} // namespace meta

namespace hesiod
{
/**
 * @brief Converts a legacy node JSON object into a CoherentNoise node JSON if it is
 *        a former noise node (NoiseFbm, NoiseRidged, NoiseIq, NoiseJordan,
 *        NoiseParberry, NoisePingpong, NoiseSwiss).
 * @param json_node The serialized node JSON object.
 * @return A converted node JSON object.
 */
nlohmann::json convert_legacy_node_json(const nlohmann::json &json_node);

/**
 * @brief Converts a legacy serialized JSON attribute object into a standard
 *        Meta-compatible JSON attribute object based on its C++ attribute type.
 * @param attr The attribute instance.
 * @param j The legacy attribute JSON object.
 * @return A Meta-compatible JSON representation of the attribute.
 */
nlohmann::json convert_legacy_attribute_json(const meta::AbstractAttribute *attr,
                                             const nlohmann::json          &j);

/**
 * @brief Converts legacy node attribute JSON (which has flat attributes and no
 *        "containers" key) into a format expected by meta::ContainerGroup::json_from.
 * @param group The ContainerGroup instance to inspect attributes for legacy conversions.
 * @param j The serialized node JSON or container JSON.
 * @return A ContainerGroup-compatible JSON representation.
 */
nlohmann::json convert_legacy_container_group_json(const meta::ContainerGroup &group,
                                                   const nlohmann::json       &j);
} // namespace hesiod
