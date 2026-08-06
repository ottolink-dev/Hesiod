#include "hesiod/model/nodes/legacy/legacy_converter.hpp"
#include <algorithm>

namespace hesiod
{

nlohmann::json convert_legacy_attribute_json(const std::string &legacy_type,
                                             const nlohmann::json &j)
{
  if (!j.is_object())
    return j;

  nlohmann::json converted = j;

  // 1. Cloud conversion: parallel arrays (x, y, values) -> vector of vec3 objects
  if (legacy_type == "Cloud" && j.contains("x") && j.contains("y") && j.contains("values"))
  {
    try
    {
      auto x = j.at("x").get<std::vector<float>>();
      auto y = j.at("y").get<std::vector<float>>();
      auto v = j.at("values").get<std::vector<float>>();

      nlohmann::json val_arr = nlohmann::json::array();
      size_t limit = std::min({x.size(), y.size(), v.size()});
      for (size_t i = 0; i < limit; ++i)
      {
        val_arr.push_back({{"x", x[i]}, {"y", y[i]}, {"z", v[i]}});
      }
      converted = nlohmann::json::object();
      converted["value"] = val_arr;
    }
    catch (...) {}
  }
  // 2. Range / Wavenumber / Vector 2D conversion: array of size 2 -> {"x": min, "y": max}
  else if ((legacy_type == "Value range" || legacy_type == "Wavenumber" || legacy_type == "Vector 2D") &&
           j.contains("value") && j["value"].is_array() && j["value"].size() == 2)
  {
    try
    {
      converted["value"] = {{"x", j["value"][0]}, {"y", j["value"][1]}};
    }
    catch (...) {}
  }
  // 3. Color conversion: array of size 4 -> {"x": r, "y": g, "z": b, "w": a}
  else if (legacy_type == "Color" && j.contains("value") && j["value"].is_array() && j["value"].size() == 4)
  {
    try
    {
      converted["value"] = {
        {"x", j["value"][0]},
        {"y", j["value"][1]},
        {"z", j["value"][2]},
        {"w", j["value"][3]}
      };
    }
    catch (...) {}
  }
  // 4. Array conversion: parallel keys ("shape.x", "shape.y", "vector") -> {"value": {"shape.x": x, "shape.y": y, "vector": [...]}}
  else if (legacy_type == "Array" && j.contains("shape.x") && j.contains("shape.y") && j.contains("vector"))
  {
    converted = nlohmann::json::object();
    converted["value"] = {
      {"shape.x", j["shape.x"]},
      {"shape.y", j["shape.y"]},
      {"vector", j["vector"]}
    };
  }

  // 4. Translate legacy metadata fields to Meta's native metadata structure
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

} // namespace hesiod
