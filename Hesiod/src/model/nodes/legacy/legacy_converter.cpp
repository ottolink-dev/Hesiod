#include <algorithm>
#include <unordered_set>
#include <vector>

#include <glm/glm.hpp>

#include "meta/core/attribute.hpp"
#include "meta/core/container_group.hpp"
#include "meta/ext/array/array.hpp"
#include "meta/ext/color_gradient/color_gradient.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/legacy/legacy_converter.hpp"

namespace hesiod
{

namespace
{

bool is_output_port_virtual_array(const std::string &node_label)
{
  const nlohmann::json &docs = HSD_CTX.node_documentation;
  if (!docs.is_object() || !docs.contains(node_label))
    return false;

  const auto &node_doc = docs[node_label];
  if (!node_doc.is_object() || !node_doc.contains("ports") ||
      !node_doc["ports"].is_object())
    return false;

  const auto &ports = node_doc["ports"];
  if (ports.contains("output") && ports["output"].is_object())
  {
    const auto &p = ports["output"];
    if (p.value("type", "") == "output" && p.value("data_type", "") == "VirtualArray")
      return true;
  }
  if (ports.contains("out") && ports["out"].is_object())
  {
    const auto &p = ports["out"];
    if (p.value("type", "") == "output" && p.value("data_type", "") == "VirtualArray")
      return true;
  }

  return false;
}

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

  // Bump mappings
  if (label == "Bump")
    return "Cosine";
  if (label == "BumpLorentzian")
    return "Lorentzian";

  // Cone mappings
  if (label == "Cone")
    return "Simple";
  if (label == "ConeComplex")
    return "Complex";
  if (label == "ConeSigmoid")
    return "Sigmoid";

  // Path mappings
  if (label == "PathResample" || label == "PathBezier" || label == "PathBezierRound" ||
      label == "PathBspline" || label == "PathDecasteljau")
    return "Interpolate";
  if (label == "PathDecimate")
    return "Decimate";
  if (label == "PathSmooth")
    return "Smooth";

  if (label == "PathNoise" || label == "PathFractalize")
    return "Fractalize";
  if (label == "PathMeanderize")
    return "Meanderize";
  if (label == "PathShuffle")
    return "Shuffle";

  if (label == "PathTransform" || label == "PathScale")
    return "Scale";
  if (label == "PathInflate")
    return "Inflate";

  if (group.current_container_name().has_value() &&
      group.contains(*group.current_container_name()))
  {
    return *group.current_container_name();
  }

  if (!group.insertion_order().empty())
    return group.insertion_order().front();

  return "main";
}

void rename_out_to_output(nlohmann::json &j, const std::string &node_label)
{
  if (!j.is_object())
    return;

  // only rename if the output port is of type VirtualArray
  if (!is_output_port_virtual_array(node_label))
    return;

  if (j.contains("containers") && j["containers"].is_object())
  {
    for (auto &[cname, cjson] : j["containers"].items())
    {
      if (cjson.is_object() && cjson.contains("out") && !cjson.contains("output"))
      {
        cjson["output"] = cjson["out"];
        cjson.erase("out");
      }
    }
  }
  else
  {
    if (j.contains("out") && !j.contains("output"))
    {
      j["output"] = j["out"];
      j.erase("out");
    }
  }
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
  // --- Bump family ---
  else if (label == "Bump")
  {
    target_label = "Bump";
    group_name = "Cosine";
  }
  else if (label == "BumpLorentzian")
  {
    target_label = "Bump";
    group_name = "Lorentzian";
  }
  // --- Cone family ---
  else if (label == "Cone")
  {
    target_label = "Cone";
    group_name = "Simple";
  }
  else if (label == "ConeComplex")
  {
    target_label = "Cone";
    group_name = "Complex";
  }
  else if (label == "ConeSigmoid")
  {
    target_label = "Cone";
    group_name = "Sigmoid";
  }
  // --- Path Resample family ---
  else if (label == "PathResample" || label == "PathBezier" ||
           label == "PathBezierRound" || label == "PathBspline" ||
           label == "PathDecasteljau")
  {
    target_label = "PathResample";
    group_name = "Interpolate";
  }
  else if (label == "PathDecimate")
  {
    target_label = "PathResample";
    group_name = "Decimate";
  }
  else if (label == "PathSmooth")
  {
    target_label = "PathResample";
    group_name = "Smooth";
  }
  // --- Path Noise family ---
  else if (label == "PathNoise" || label == "PathFractalize")
  {
    target_label = "PathNoise";
    group_name = "Fractalize";
  }
  else if (label == "PathMeanderize")
  {
    target_label = "PathNoise";
    group_name = "Meanderize";
  }
  else if (label == "PathShuffle")
  {
    target_label = "PathNoise";
    group_name = "Shuffle";
  }
  // --- Path Transform family ---
  else if (label == "PathTransform" || label == "PathScale")
  {
    target_label = "PathTransform";
    group_name = "Scale";
  }
  else if (label == "PathInflate")
  {
    target_label = "PathTransform";
    group_name = "Inflate";
  }
  // --- SetBorders ---
  else if (label == "SetBorders")
  {
    nlohmann::json converted_node = json_node;
    if (converted_node.contains("containers") && converted_node["containers"].is_object())
    {
      if (converted_node["containers"].contains("main") &&
          converted_node["containers"]["main"].is_object())
      {
        auto &main_json = converted_node["containers"]["main"];
        if (!main_json.contains("value") && main_json.contains("value_west"))
        {
          main_json["value"] = main_json["value_west"];
        }
      }
    }
    else
    {
      if (!converted_node.contains("value") && converted_node.contains("value_west"))
      {
        converted_node["value"] = converted_node["value_west"];
      }
    }
    rename_out_to_output(converted_node, "SetBorders");
    return converted_node;
  }

  if (target_label.empty() || group_name.empty())
  {
    nlohmann::json converted_node = json_node;
    rename_out_to_output(converted_node, label);
    return converted_node;
  }

  nlohmann::json converted_node = json_node;
  converted_node["label"] = target_label;

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
      converted_node["current"] = group_name;
    }
  }
  else
  {
    converted_node["current"] = group_name;
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

  rename_out_to_output(converted_node, target_label);
  return converted_node;
}

nlohmann::json convert_legacy_attribute_json(const meta::AbstractAttribute *attr,
                                             const nlohmann::json          &j)
{
  if (!attr)
    return j;

  nlohmann::json converted = j;
  if (j.is_array() || j.is_number() || j.is_string() || j.is_boolean())
  {
    converted = nlohmann::json::object();
    converted["value"] = j;
  }
  else if (!j.is_object())
  {
    return j;
  }

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
    else if (!j.contains("value") && j.contains("x") && j.contains("y"))
    {
      converted["value"] = {{"x", j["x"]}, {"y", j["y"]}};
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

  // Translate legacy metadata and state fields
  if (j.contains("is_active"))
  {
    converted["state"]["active"]["value"] = j["is_active"];
    converted["metadata"]["ui.active"]["value"] = j["is_active"];
  }
  if (j.contains("link_xy"))
  {
    converted["state"]["locked_xy"]["value"] = j["link_xy"];
    converted["metadata"]["ui.locked_xy"]["value"] = j["link_xy"];
  }
  if (j.contains("state") && j["state"].is_object())
  {
    if (j["state"].contains("active") && !j["state"]["active"].is_object())
    {
      converted["state"]["active"] = {{"value", j["state"]["active"]}};
    }
    if (j["state"].contains("locked_xy") && !j["state"]["locked_xy"].is_object())
    {
      converted["state"]["locked_xy"] = {{"value", j["state"]["locked_xy"]}};
    }
  }
  if (j.contains("metadata") && j["metadata"].is_object())
  {
    if (j["metadata"].contains("ui.active"))
    {
      auto ui_act = j["metadata"]["ui.active"];
      if (ui_act.is_object() && ui_act.contains("value"))
        converted["state"]["active"]["value"] = ui_act["value"];
      else if (ui_act.is_boolean())
        converted["state"]["active"]["value"] = ui_act;
    }
    if (j["metadata"].contains("ui.locked_xy"))
    {
      auto ui_lock = j["metadata"]["ui.locked_xy"];
      if (ui_lock.is_object() && ui_lock.contains("value"))
        converted["state"]["locked_xy"]["value"] = ui_lock["value"];
      else if (ui_lock.is_boolean())
        converted["state"]["locked_xy"]["value"] = ui_lock;
    }
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
          if (container->find("value") && !cjson.contains("value") &&
              cjson.contains("value_west"))
          {
            cjson["value"] = cjson["value_west"];
          }

          if (container->find("output") && !cjson.contains("output") &&
              cjson.contains("out"))
          {
            cjson["output"] = cjson["out"];
          }

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
    nlohmann::json j_copy = j;
    if (container->find("value") && !j_copy.contains("value") &&
        j_copy.contains("value_west"))
    {
      j_copy["value"] = j_copy["value_west"];
    }

    if (container->find("output") && !j_copy.contains("output") && j_copy.contains("out"))
    {
      j_copy["output"] = j_copy["out"];
    }

    for (const auto &key : container->insertion_order())
    {
      auto *attr = container->find(key);
      if (j_copy.contains(key))
      {
        target_container_json[key] = convert_legacy_attribute_json(attr, j_copy[key]);
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

static bool is_path_modifier_node(const std::string &label)
{
  return label == "PathResample" || label == "PathBezier" || label == "PathBezierRound" ||
         label == "PathBspline" || label == "PathDecasteljau" ||
         label == "PathDecimate" || label == "PathSmooth" || label == "PathNoise" ||
         label == "PathFractalize" || label == "PathMeanderize" ||
         label == "PathShuffle" || label == "PathTransform" || label == "PathScale" ||
         label == "PathInflate";
}

nlohmann::json convert_legacy_graph_json(const nlohmann::json &graph_json)
{
  if (!graph_json.is_object())
    return graph_json;

  nlohmann::json                               converted_graph = graph_json;
  std::unordered_map<std::string, std::string> node_labels;

  if (converted_graph.contains("nodes") && converted_graph["nodes"].is_array())
  {
    for (auto &json_node : converted_graph["nodes"])
    {
      std::string id = "";
      std::string label = "";
      if (json_node.contains("id") && json_node["id"].is_string())
        id = json_node["id"].get<std::string>();
      if (json_node.contains("label") && json_node["label"].is_string())
        label = json_node["label"].get<std::string>();

      node_labels[id] = label;
      json_node = convert_legacy_node_json(json_node);
    }
  }

  if (converted_graph.contains("links") && converted_graph["links"].is_array())
  {
    for (auto &json_link : converted_graph["links"])
    {
      if (!json_link.is_object())
        continue;

      std::string node_id_from = json_link.value("node_id_from", "");
      std::string port_id_from = json_link.value("port_id_from", "");
      std::string node_id_to = json_link.value("node_id_to", "");
      std::string port_id_to = json_link.value("port_id_to", "");

      std::string from_label = node_labels[node_id_from];
      std::string to_label = node_labels[node_id_to];

      if (port_id_from == "out")
      {
        json_link["port_id_from"] = "output";
      }
      else if (port_id_from == "path" && is_path_modifier_node(from_label))
      {
        json_link["port_id_from"] = "output";
      }

      if (port_id_to == "in")
      {
        json_link["port_id_to"] = "input";
      }
      else if (port_id_to == "path" && is_path_modifier_node(to_label))
      {
        json_link["port_id_to"] = "input";
      }
    }
  }

  return converted_graph;
}

nlohmann::json convert_legacy_graph_widget_json(const nlohmann::json &widget_json)
{
  if (!widget_json.is_object())
    return widget_json;

  nlohmann::json                               converted_widget = widget_json;
  std::unordered_map<std::string, std::string> node_captions;

  if (converted_widget.contains("nodes") && converted_widget["nodes"].is_array())
  {
    for (const auto &json_node : converted_widget["nodes"])
    {
      std::string id = "";
      std::string caption = "";
      if (json_node.contains("id") && json_node["id"].is_string())
        id = json_node["id"].get<std::string>();
      if (json_node.contains("caption") && json_node["caption"].is_string())
        caption = json_node["caption"].get<std::string>();

      node_captions[id] = caption;
    }
  }

  if (converted_widget.contains("links") && converted_widget["links"].is_array())
  {
    for (auto &json_link : converted_widget["links"])
    {
      if (!json_link.is_object())
        continue;

      std::string node_out_id = json_link.value("node_out_id", "");
      std::string port_out_id = json_link.value("port_out_id", "");
      std::string node_in_id = json_link.value("node_in_id", "");
      std::string port_in_id = json_link.value("port_in_id", "");

      std::string from_label = node_captions[node_out_id];
      std::string to_label = node_captions[node_in_id];

      if (port_out_id == "out")
      {
        json_link["port_out_id"] = "output";
      }
      else if (port_out_id == "path" && is_path_modifier_node(from_label))
      {
        json_link["port_out_id"] = "output";
      }

      if (port_in_id == "in")
      {
        json_link["port_in_id"] = "input";
      }
      else if (port_in_id == "path" && is_path_modifier_node(to_label))
      {
        json_link["port_in_id"] = "input";
      }
    }
  }

  return converted_widget;
}

} // namespace hesiod
