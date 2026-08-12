#include <algorithm>
#include <vector>

#include <glm/glm.hpp>

#include "meta/core/attribute.hpp"
#include "meta/ext/array/array.hpp"
#include "meta/ext/color_gradient/color_gradient.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/legacy/legacy_converter.hpp"

namespace hesiod
{

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

} // namespace hesiod
