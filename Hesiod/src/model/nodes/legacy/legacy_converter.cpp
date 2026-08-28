#include <algorithm>
#include <unordered_set>
#include <vector>

#include <glm/glm.hpp>

#include "meta/core/attribute.hpp"
#include "meta/core/container_group.hpp"
#include "meta/ext/array/array.hpp"
#include "meta/ext/color_gradient/color_gradient.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/legacy/legacy_converter.hpp"

namespace hesiod
{

namespace
{

std::string resolve_legacy_group_name(const meta::ContainerGroup &group,
                                      const nlohmann::json       &j)
{
  std::string label;
  if (j.contains("label") && j["label"].is_string())
    label = j["label"].get<std::string>();

  // Coherent noise mappings
  if (label == "Noise" || label == "NoiseFbm")
    return "FBM";
  if (label == "NoiseRidged")
    return "Ridged";
  if (label == "NoiseIq")
    return "IQ";
  if (label == "NoiseJordan")
    return "Jordan";
  if (label == "NoiseParberry")
    return "Parberry";
  if (label == "NoisePingpong")
    return "PingPong";
  if (label == "NoiseSwiss")
    return "Swiss";

  // Cellular noise mappings
  if (label == "Voronoi" || label == "VoronoiFbm")
    return "Grid";
  if (label == "Vorolines" || label == "VorolinesFbm")
    return "Lines";
  if (label == "Vororand")
    return "Scattered";
  if (label == "Voronoise")
    return "Voronoise";

  // Path fractalize mappings
  if (label == "PathFractalize")
    return "Fractalize";

  if (group.current_container_name().has_value() &&
      group.contains(*group.current_container_name()))
  {
    return *group.current_container_name();
  }

  if (!group.insertion_order().empty())
    return group.insertion_order().front();

  return "main";
}

} // namespace

nlohmann::json convert_legacy_node_json(const nlohmann::json &json_node)
{
  if (!json_node.is_object())
    return json_node;

  std::string label;
  if (json_node.contains("label") && json_node["label"].is_string())
    label = json_node["label"].get<std::string>();

  std::string target_label;
  std::string group_name;
  bool        is_single_octave = false;

  // --- Coherent noise family ---
  if (label == "Noise")
  {
    target_label = "CoherentNoise";
    group_name = "FBM";
    is_single_octave = true;
  }
  else if (label == "NoiseFbm")
  {
    target_label = "CoherentNoise";
    group_name = "FBM";
  }
  else if (label == "NoiseRidged")
  {
    target_label = "CoherentNoise";
    group_name = "Ridged";
  }
  else if (label == "NoiseIq")
  {
    target_label = "CoherentNoise";
    group_name = "IQ";
  }
  else if (label == "NoiseJordan")
  {
    target_label = "CoherentNoise";
    group_name = "Jordan";
  }
  else if (label == "NoiseParberry")
  {
    target_label = "CoherentNoise";
    group_name = "Parberry";
  }
  else if (label == "NoisePingpong")
  {
    target_label = "CoherentNoise";
    group_name = "PingPong";
  }
  else if (label == "NoiseSwiss")
  {
    target_label = "CoherentNoise";
    group_name = "Swiss";
  }
  // --- Cellular noise / Voronoi family ---
  else if (label == "Voronoi")
  {
    target_label = "CellularNoise";
    group_name = "Grid";
    is_single_octave = true;
  }
  else if (label == "VoronoiFbm")
  {
    target_label = "CellularNoise";
    group_name = "Grid";
  }
  else if (label == "Vorolines")
  {
    target_label = "CellularNoise";
    group_name = "Lines";
    is_single_octave = true;
  }
  else if (label == "VorolinesFbm")
  {
    target_label = "CellularNoise";
    group_name = "Lines";
  }
  else if (label == "Vororand")
  {
    target_label = "CellularNoise";
    group_name = "Scattered";
  }
  else if (label == "Voronoise")
  {
    target_label = "CellularNoise";
    group_name = "Voronoise";
  }
  // --- Path Fractalize ---
  else if (label == "PathFractalize")
  {
    target_label = "PathFractalize";
    group_name = "Fractalize";
  }

  if (target_label.empty() || group_name.empty())
    return json_node;

  nlohmann::json converted_node = json_node;
  converted_node["label"] = target_label;
  converted_node["current"] = group_name;

  // If the node already has a "containers" object
  if (converted_node.contains("containers") && converted_node["containers"].is_object())
  {
    if (converted_node["containers"].contains("main"))
    {
      nlohmann::json main_json = converted_node["containers"]["main"];
      converted_node["containers"].erase("main");
      if (is_single_octave)
      {
        main_json["octaves"] = {{"value", 1}};
        if (!main_json.contains("weight"))
          main_json["weight"] = {{"value", 0.7f}};
        if (!main_json.contains("persistence"))
          main_json["persistence"] = {{"value", 0.5f}};
        if (!main_json.contains("lacunarity"))
          main_json["lacunarity"] = {{"value", 2.0f}};
      }
      converted_node["containers"][group_name] = main_json;
    }
  }
  else
  {
    // Legacy format: flat attributes at the node level
    static const std::unordered_set<std::string> node_keys = {"id",
                                                              "label",
                                                              "caption",
                                                              "comment",
                                                              "runtime_info",
                                                              "state",
                                                              "current",
                                                              "containers"};

    nlohmann::json           container_json = nlohmann::json::object();
    std::vector<std::string> keys_to_remove;

    for (auto &[key, val] : converted_node.items())
    {
      if (!node_keys.contains(key))
      {
        container_json[key] = val;
        keys_to_remove.push_back(key);
      }
    }

    for (const auto &key : keys_to_remove)
    {
      converted_node.erase(key);
    }

    if (converted_node.contains("state"))
    {
      container_json["state"] = converted_node["state"];
    }

    if (is_single_octave)
    {
      container_json["octaves"] = {{"value", 1}};
      if (!container_json.contains("weight"))
        container_json["weight"] = {{"value", 0.7f}};
      if (!container_json.contains("persistence"))
        container_json["persistence"] = {{"value", 0.5f}};
      if (!container_json.contains("lacunarity"))
        container_json["lacunarity"] = {{"value", 2.0f}};
    }

    converted_node["containers"] = nlohmann::json::object();
    converted_node["containers"][group_name] = container_json;
  }

  return converted_node;
}

nlohmann::json convert_legacy_attribute_json(const meta::AbstractAttribute *attr,
                                             const nlohmann::json          &j)
{
  if (!j.is_object() || !attr)
    return j;

  nlohmann::json converted = j;

  // Cloud conversion: meta::Attribute<std::vector<glm::vec3>>
  if (attr->try_cast<meta::Attribute<std::vector<glm::vec3>>>())
  {
    if (j.contains("x") && j.contains("y") && j.contains("values"))
    {
      try
      {
        auto x = j.at("x").get<std::vector<float>>();
        auto y = j.at("y").get<std::vector<float>>();
        auto v = j.at("values").get<std::vector<float>>();

        nlohmann::json val_arr = nlohmann::json::array();
        size_t         limit = std::min({x.size(), y.size(), v.size()});
        for (size_t i = 0; i < limit; ++i)
        {
          val_arr.push_back({{"x", x[i]}, {"y", y[i]}, {"z", v[i]}});
        }
        converted = nlohmann::json::object();
        converted["value"] = val_arr;
      }
      catch (const std::exception &e)
      {
        Logger::log()->warn("Legacy converter: Cloud conversion failed: {}", e.what());
      }
    }
  }
  // Range / Wavenumber / Vector 2D conversion: meta::Attribute<glm::vec2>
  else if (attr->try_cast<meta::Attribute<glm::vec2>>())
  {
    if (j.contains("value") && j["value"].is_array() && j["value"].size() == 2)
    {
      try
      {
        converted["value"] = {{"x", j["value"][0]}, {"y", j["value"][1]}};
      }
      catch (const std::exception &e)
      {
        Logger::log()->warn("Legacy converter: Vec2 conversion failed: {}", e.what());
      }
    }
  }
  // Color conversion: meta::Attribute<glm::vec4>
  else if (attr->try_cast<meta::Attribute<glm::vec4>>())
  {
    if (j.contains("value") && j["value"].is_array() && j["value"].size() == 4)
    {
      try
      {
        converted["value"] = {{"x", j["value"][0]},
                              {"y", j["value"][1]},
                              {"z", j["value"][2]},
                              {"w", j["value"][3]}};
      }
      catch (const std::exception &e)
      {
        Logger::log()->warn("Legacy converter: Vec4 conversion failed: {}", e.what());
      }
    }
  }
  // Array conversion: meta::Attribute<meta::Array>
  else if (attr->try_cast<meta::Attribute<meta::Array>>())
  {
    if (j.contains("shape.x") && j.contains("shape.y"))
    {
      try
      {
        size_t             shape_x = j.at("shape.x").get<size_t>();
        size_t             shape_y = j.at("shape.y").get<size_t>();
        size_t             expected_size = shape_x * shape_y;
        std::vector<float> vec;
        bool               size_issue = false;

        try
        {
          if (j.contains("vector") && j.at("vector").is_array())
          {
            vec = j.at("vector").get<std::vector<float>>();
          }
          else
          {
            size_issue = true;
          }
        }
        catch (...)
        {
          size_issue = true;
        }

        if (vec.size() != expected_size)
        {
          size_issue = true;
        }

        if (size_issue)
        {
          Logger::log()->warn(
              "Legacy converter: Array size mismatch or malformed vector (shape: "
              "{}x{}={}, vector size: {}). Padding/truncating with zeros.",
              shape_x,
              shape_y,
              expected_size,
              vec.size());
          vec.resize(expected_size, 0.0f);
        }

        // meta::Array::json_from expects a nested "shape" object (see
        // Array::json_to); emitting flat "shape.x"/"shape.y" keys leaves the
        // shape at its reset value of {0, 0}, which silently discards the
        // vector and yields a zero-sized array downstream.
        nlohmann::json value = nlohmann::json::object();
        value["shape"]["x"] = shape_x;
        value["shape"]["y"] = shape_y;
        value["vector"] = vec;

        converted = nlohmann::json::object();
        converted["value"] = value;
      }
      catch (const std::exception &e)
      {
        Logger::log()->warn("Legacy converter: Array conversion failed: {}", e.what());
      }
    }
  }
  // ColorGradient conversion: meta::Attribute<meta::ColorGradient>
  else if (attr->try_cast<meta::Attribute<meta::ColorGradient>>())
  {
    if (j.contains("value") && j["value"].is_array())
    {
      try
      {
        converted = nlohmann::json::object();
        converted["value"] = nlohmann::json::object();
        converted["value"]["value"] = j["value"];
      }
      catch (const std::exception &e)
      {
        Logger::log()->warn("Legacy converter: ColorGradient conversion failed: {}",
                            e.what());
      }
    }
  }

  // Translate legacy metadata fields to Meta's native metadata structure
  if (j.contains("is_active"))
  {
    converted["metadata"]["ui.active"]["value"] = j["is_active"];
  }
  if (j.contains("link_xy"))
  {
    converted["metadata"]["ui.locked_xy"]["value"] = j["link_xy"];
  }

  return converted;
}

nlohmann::json convert_legacy_container_group_json(const meta::ContainerGroup &group,
                                                   const nlohmann::json       &j)
{
  if (!j.is_object())
    return j;

  // If already in ContainerGroup format (has "containers"), convert container contents if
  // needed
  if (j.contains("containers") && j["containers"].is_object())
  {
    nlohmann::json converted_group = j;

    // If the serialized group contains a single "main" container but the target group
    // does not have a "main" container (e.g. CoherentNoise, CellularNoise), remap "main"
    // to the resolved group container name.
    if (converted_group["containers"].contains("main") && !group.contains("main"))
    {
      std::string    target_name = resolve_legacy_group_name(group, j);
      nlohmann::json main_json = converted_group["containers"]["main"];
      converted_group["containers"].erase("main");
      converted_group["containers"][target_name] = main_json;
      converted_group["current"] = target_name;
    }

    for (auto &[cname, cjson] : converted_group["containers"].items())
    {
      if (cjson.is_object())
      {
        const meta::AttributeContainer *container = group.find(cname);
        if (!container)
        {
          container = &group.current();
        }

        if (container)
        {
          for (const auto &key : container->insertion_order())
          {
            auto *attr = container->find(key);
            if (cjson.contains(key))
            {
              cjson[key] = convert_legacy_attribute_json(attr, cjson[key]);
            }
          }
        }
      }
    }
    return converted_group;
  }

  // Legacy format: the node JSON has attributes directly (flat attributes).
  // Determine target container name (either "main" if present in group, or resolved group
  // name for multi-container nodes like CoherentNoise, CellularNoise).
  std::string target_container_name = "main";
  if (!group.contains("main"))
  {
    target_container_name = resolve_legacy_group_name(group, j);
  }

  nlohmann::json group_json = nlohmann::json::object();
  group_json["current"] = target_container_name;

  nlohmann::json target_container_json = nlohmann::json::object();

  const meta::AttributeContainer *container = group.find(target_container_name);
  if (!container)
    container = &group.current();

  if (container)
  {
    for (const auto &key : container->insertion_order())
    {
      auto *attr = container->find(key);
      if (j.contains(key))
      {
        target_container_json[key] = convert_legacy_attribute_json(attr, j[key]);
      }
    }
  }
  else
  {
    // Fallback: copy keys that are not standard node keys
    static const std::unordered_set<std::string> node_keys = {"id",
                                                              "label",
                                                              "comment",
                                                              "runtime_info",
                                                              "state"};
    for (auto &[key, val] : j.items())
    {
      if (!node_keys.contains(key))
      {
        target_container_json[key] = val;
      }
    }
  }

  if (j.contains("state"))
  {
    target_container_json["state"] = j["state"];
  }

  group_json["containers"][target_container_name] = target_container_json;
  return group_json;
}

} // namespace hesiod
