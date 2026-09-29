/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <format>
#include <random>
#include <set>

#include "highmap/geometry/cloud.hpp"
#include "highmap/geometry/path.hpp"
#include "highmap/virtual_array/virtual_texture.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/graph/graph_node.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/macro_node.hpp"
#include "hesiod/model/nodes/node_factory.hpp"

namespace hesiod
{

namespace
{

// data kinds a macro port can carry, by node type suffix
template <typename T> struct Kind
{
  using type = T;
  const char *suffix;
};

template <typename F> bool visit_kinds(F &&f)
{
  return f(Kind<hmap::VirtualArray>{"Heightmap"}) ||
         f(Kind<hmap::VirtualTexture>{"Texture"}) || f(Kind<hmap::Cloud>{"Cloud"}) ||
         f(Kind<hmap::Path>{"Path"}) || f(Kind<hmap::Array>{"Kernel"});
}

// calls f(Kind<T>) for the kind whose typeid name is `data_type`
template <typename F> bool visit_data_type(const std::string &data_type, F &&f)
{
  return visit_kinds(
      [&](auto kind)
      {
        if (data_type != typeid(typename decltype(kind)::type).name())
          return false;
        f(kind);
        return true;
      });
}

template <typename T>
void add_data_port(BaseNode &node, gnode::PortType direction, const std::string &label)
{
  if (direction == gnode::PortType::IN)
    node.add_port<T>(direction, label);
  else if constexpr (std::is_same_v<T, hmap::VirtualArray>)
    node.add_port<T>(direction, label, CONFIG(node));
  else if constexpr (std::is_same_v<T, hmap::VirtualTexture>)
    node.add_port<T>(direction, label, CONFIG_TEX(node));
  else if constexpr (std::is_same_v<T, hmap::Array>)
    node.add_port<T>(direction, label, node.cfg().shape);
  else
    node.add_port<T>(direction, label);
}

} // namespace

bool is_macro_io_node_type(const std::string &node_type)
{
  return node_type.starts_with("MacroInput") || node_type.starts_with("MacroOutput");
}

bool is_macro_body(const GraphNode &graph)
{
  return graph.get_id().starts_with("macro:");
}

std::map<std::string, std::string> node_inventory_for(const GraphNode &graph)
{
  std::map<std::string, std::string> inventory = get_node_inventory();
  if (!is_macro_body(graph))
    std::erase_if(inventory,
                  [](const auto &entry) { return is_macro_io_node_type(entry.first); });
  return inventory;
}

std::string macro_io_data_type(const std::string &node_type)
{
  const std::string suffix = node_type.substr(node_type.starts_with("MacroInput") ? 10
                                                                                  : 11);
  std::string       data_type;
  visit_kinds(
      [&](auto kind)
      {
        if (suffix != kind.suffix)
          return false;
        data_type = typeid(typename decltype(kind)::type).name();
        return true;
      });
  return data_type;
}

void setup_macro_io_node(BaseNode &node)
{
  const std::string type = node.get_node_type();
  const bool        is_input = type.starts_with("MacroInput");
  const std::string data_type = macro_io_data_type(type);

  visit_data_type(data_type,
                  [&](auto kind)
                  {
                    using T = typename decltype(kind)::type;
                    if (is_input)
                      add_data_port<T>(node, gnode::PortType::OUT, "output");
                    else
                      add_data_port<T>(node, gnode::PortType::IN, "input");
                  });

  node.set_current_category("Main");
  add_string(node, "name", "Port name", is_input ? "input" : "output");
  add_int(node, "order", "Order", 0, 0, 16);
  if (is_input && data_type == typeid(hmap::VirtualArray).name())
    add_float(node, "fallback", "Value when not connected", 0.f, -1.f, 1.f);
}

} // namespace hesiod
