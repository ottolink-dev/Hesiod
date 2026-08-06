#include "hesiod/model/nodes/legacy/legacy_converter.hpp"
#include "meta/core/attribute.hpp"
#include "meta/ext/array/array.hpp"
#include <algorithm>
#include <glm/glm.hpp>
#include <vector>

namespace hesiod
{

nlohmann::json convert_legacy_attribute_json(const meta::AbstractAttribute *attr,
                                             const nlohmann::json          &j)
{
  if (!j.is_object() || !attr)
    return j;

  nlohmann::json converted = j;

  // 1. Cloud conversion: meta::Attribute<std::vector<glm::vec3>>
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
      catch (...)
      {
      }
    }
  }
  // 2. Range / Wavenumber / Vector 2D conversion: meta::Attribute<glm::vec2>
  else if (attr->try_cast<meta::Attribute<glm::vec2>>())
  {
    if (j.contains("value") && j["value"].is_array() && j["value"].size() == 2)
    {
      try
      {
        converted["value"] = {{"x", j["value"][0]}, {"y", j["value"][1]}};
      }
      catch (...)
      {
      }
    }
  }
  // 3. Color conversion: meta::Attribute<glm::vec4>
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
      catch (...)
      {
      }
    }
  }
  // 4. Array conversion: meta::Attribute<meta::Array>
  else if (attr->try_cast<meta::Attribute<meta::Array>>())
  {
    if (j.contains("shape.x") && j.contains("shape.y") && j.contains("vector"))
    {
      converted = nlohmann::json::object();
      converted["value"] = {{"shape.x", j["shape.x"]},
                            {"shape.y", j["shape.y"]},
                            {"vector", j["vector"]}};
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

} // namespace hesiod
