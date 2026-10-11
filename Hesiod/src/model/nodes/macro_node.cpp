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

std::string suffix_of(const std::string &data_type)
{
  std::string suffix;
  visit_data_type(data_type, [&](auto kind) { suffix = kind.suffix; });
  return suffix;
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

template <typename T> void copy_port_data(T &dst, T &src, const hmap::ComputeMode &cm)
{
  if constexpr (std::is_same_v<T, hmap::VirtualArray> ||
                std::is_same_v<T, hmap::VirtualTexture>)
    dst.copy_from(src, cm);
  else
    dst = src;
}

template <typename T>
void clear_port_data(T &dst, float value, const hmap::ComputeMode &cm)
{
  if constexpr (std::is_same_v<T, hmap::VirtualArray>)
    dst.fill(value, cm);
  else if constexpr (std::is_same_v<T, hmap::VirtualTexture>)
    dst.fill(0.f, cm);
  else if constexpr (std::is_same_v<T, hmap::Array>)
    dst = hmap::Array(dst.shape, 0.f);
  else
    dst = T();
}

bool numeric_less(const std::string &a, const std::string &b)
{
  // node ids are counters: "9" comes before "10"
  return a.size() != b.size() ? a.size() < b.size() : a < b;
}

std::string unique_name(std::string base, std::set<std::string> &used)
{
  if (base.empty())
    base = "port";
  std::string name = base;
  for (int k = 2; used.contains(name); ++k)
    name = base + " " + std::to_string(k);
  used.insert(name);
  return name;
}

// the interface of a body: inputs then outputs, each by order then creation
std::vector<MacroPort> body_ports(GraphNode &body)
{
  struct Entry
  {
    MacroPort port;
    int       order;
  };
  std::vector<Entry> entries;

  for (const auto &[id, p_node] : body.get_nodes())
  {
    auto *node = dynamic_cast<BaseNode *>(p_node.get());
    if (!node || !is_macro_io_node_type(node->get_node_type()) || node->get_nports() < 1)
      continue;

    MacroPort port;
    port.name = node->val<std::string>("name");
    port.node_id = id;
    port.data_type = node->get_data_type(0);
    port.is_input = node->get_node_type().starts_with("MacroInput");
    entries.push_back({port, node->val<int>("order")});
  }

  std::sort(entries.begin(),
            entries.end(),
            [](const Entry &a, const Entry &b)
            {
              if (a.port.is_input != b.port.is_input)
                return a.port.is_input;
              if (a.order != b.order)
                return a.order < b.order;
              return numeric_less(a.port.node_id, b.port.node_id);
            });

  std::vector<MacroPort> ports;
  std::set<std::string>  used;
  for (auto &entry : entries)
  {
    entry.port.name = unique_name(entry.port.name, used);
    ports.push_back(entry.port);
  }
  return ports;
}

bool same_interface(const std::vector<MacroPort> &a, const std::vector<MacroPort> &b)
{
  return std::equal(a.begin(),
                    a.end(),
                    b.begin(),
                    b.end(),
                    [](const MacroPort &x, const MacroPort &y)
                    {
                      return x.name == y.name && x.data_type == y.data_type &&
                             x.is_input == y.is_input;
                    });
}

// a body link, in the GraphNode::json_to format
nlohmann::json link_json(const PortLink &l)
{
  return {{"node_id_from", l.node_out},
          {"port_id_from", l.port_out},
          {"node_id_to", l.node_in},
          {"port_id_to", l.port_in}};
}

nlohmann::json ports_to_json(const std::vector<MacroPort> &ports)
{
  nlohmann::json json = nlohmann::json::array();
  for (const auto &port : ports)
    json.push_back({{"name", port.name},
                    {"node_id", port.node_id},
                    {"data_type", port.data_type},
                    {"is_input", port.is_input}});
  return json;
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

std::string new_macro_id()
{
  std::random_device                      device;
  std::mt19937_64                         engine(device());
  std::uniform_int_distribution<uint64_t> dist;
  return std::format("{:016x}", dist(engine));
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

// =====================================
// MacroNode
// =====================================

MacroNode::MacroNode(const std::string &label, std::weak_ptr<GraphConfig> config)
    : BaseNode(label, config)
{
}

void MacroNode::set_definition(const nlohmann::json &new_definition)
{
  // a definition holding an instance of itself would build forever
  thread_local std::vector<std::string> building;

  const std::string macro_id = new_definition.value("macro_id", "");
  if (std::find(building.begin(), building.end(), macro_id) != building.end())
    throw std::runtime_error("The macro '" + new_definition.value("name", macro_id) +
                             "' contains itself.");

  building.push_back(macro_id);
  struct Pop
  {
    ~Pop() { building.pop_back(); }
  } pop;

  auto config = std::make_shared<GraphConfig>(this->cfg());
  auto new_body = std::make_shared<GraphNode>("", config);

  // Receive nodes inside read the project's broadcasts, placed like the parent
  if (auto *parent = dynamic_cast<GraphNode *>(this->get_p_graph()))
  {
    new_body->set_p_broadcast_params(parent->get_p_broadcast_params());
    new_body->set_origin(parent->get_origin());
    new_body->set_size(parent->get_size());
    new_body->set_rotation_angle(parent->get_rotation_angle());
  }

  new_body->json_from(new_definition.value("graph", nlohmann::json::object()),
                      config.get());
  new_body->set_id("macro:" + new_definition.value("name", std::string("Macro")));

  this->set_body(new_body, new_definition);
}

void MacroNode::set_body(std::shared_ptr<GraphNode> new_body,
                         const nlohmann::json      &new_definition)
{
  std::vector<MacroPort> new_ports = body_ports(*new_body);

  if (!this->body)
  {
    for (const auto &port : new_ports)
      visit_data_type(port.data_type,
                      [&](auto kind)
                      {
                        using T = typename decltype(kind)::type;
                        add_data_port<T>(*this,
                                         port.is_input ? gnode::PortType::IN
                                                       : gnode::PortType::OUT,
                                         port.name);
                      });
  }
  else if (!same_interface(new_ports, this->ports_info))
  {
    throw std::logic_error("MacroNode::set_definition: the ports changed, the node "
                           "has to be rebuilt");
  }

  this->body = new_body;
  this->ports_info = new_ports;
  this->definition = new_definition;
  this->definition.erase("graph");
  this->body_stale = true;
  this->body_filled = false;
}

nlohmann::json MacroNode::get_definition() const
{
  nlohmann::json json = this->definition;
  if (this->body)
  {
    json["graph"] = this->body->json_to();
    json["ports"] = ports_to_json(body_ports(*this->body));
  }
  if (this->layout_provider)
    if (nlohmann::json layout = this->layout_provider(); layout.is_object())
      json["layout"] = layout;
  return json;
}

std::string MacroNode::get_macro_id() const
{
  return this->definition.value("macro_id", "");
}

std::string MacroNode::get_macro_name() const
{
  return this->definition.value("name", std::string("Macro"));
}

int MacroNode::get_revision() const { return this->definition.value("revision", 0); }

void MacroNode::json_from(nlohmann::json const &json)
{
  BaseNode::json_from(json);

  if (json.contains("macro") && json["macro"].is_object())
    this->set_definition(json["macro"]);
  else
    Logger::log()->error("MacroNode::json_from: node {} has no macro definition",
                         this->get_id());
}

nlohmann::json MacroNode::json_to() const
{
  nlohmann::json json = BaseNode::json_to();
  json["macro"] = this->get_definition();
  return json;
}

void MacroNode::propagate_config_change()
{
  BaseNode::propagate_config_change();

  if (!this->body)
    return;

  *this->body->get_config_ref() = this->cfg();
  for (auto &[id, p_node] : this->body->get_nodes())
    if (auto *node = dynamic_cast<BaseNode *>(p_node.get()))
      node->propagate_config_change();
  this->body_stale = true;
  this->body_filled = false;
}

void MacroNode::compute_macro()
{
  if (!this->body)
    return;

  this->computing = true;
  struct Done
  {
    bool &flag;
    ~Done() { flag = false; }
  } done{this->computing};

  if (!std::exchange(this->body_computed, false))
  {
    this->move_data(true);

    std::vector<std::string> input_ids;
    for (const auto &port : this->ports_info)
      if (port.is_input)
        input_ids.push_back(port.node_id);

    // new inputs only change what depends on them; a body filled with the
    // results of the packed nodes needs nothing at all
    if (std::exchange(this->body_filled, false))
      this->body_stale = false;
    else if (std::exchange(this->body_stale, false) || input_ids.empty())
      this->body->update();
    else
      this->body->update(input_ids);
  }

  this->move_data(false);
}

void MacroNode::take_results_from(GraphNode &graph)
{
  for (const auto &[id, p_node] : this->body->get_nodes())
  {
    auto *from = dynamic_cast<BaseNode *>(graph.get_node(id));
    auto *to = dynamic_cast<BaseNode *>(p_node.get());
    if (!from || !to || from->get_label() != to->get_label())
      continue;

    for (int k = 0; k < to->get_nports(); ++k)
      if (to->get_port_type(k) == gnode::PortType::OUT)
        visit_data_type(to->get_data_type(k),
                        [&](auto kind)
                        {
                          using T = typename decltype(kind)::type;
                          T *src = from->get_value_ref<T>(k);
                          T *dst = to->get_value_ref<T>(k);
                          if (src && dst)
                            copy_port_data(*dst, *src, this->cfg().cm_cpu);
                        });
  }

  this->body_stale = false;
  this->body_filled = true;
}

void MacroNode::move_data(bool inputs)
{
  for (const auto &port : this->ports_info)
  {
    auto *io = this->body->get_node_ref_by_id<BaseNode>(port.node_id);
    if (port.is_input != inputs || !io)
      continue;

    visit_data_type(
        port.data_type,
        [&](auto kind)
        {
          using T = typename decltype(kind)::type;
          // in: instance port -> interface node; out: interface node -> instance port
          T    *src = inputs ? this->get_value_ref<T>(port.name)
                             : io->get_value_ref<T>("input");
          T    *dst = inputs ? io->get_value_ref<T>("output")
                             : this->get_value_ref<T>(port.name);
          auto *fallback = io->attr<float>("fallback");

          if (dst && src)
            copy_port_data(*dst, *src, this->cfg().cm_cpu);
          else if (dst)
            clear_port_data(*dst, fallback ? fallback->value() : 0.f, this->cfg().cm_cpu);
        });
  }
}

// =====================================
// Functions
// =====================================

std::vector<PortLink> links_of(const GraphNode &graph)
{
  std::vector<PortLink> links;
  for (const auto &link : graph.get_links())
    links.push_back({link.from,
                     graph.get_node(link.from)->get_port_label(link.port_from),
                     link.to,
                     graph.get_node(link.to)->get_port_label(link.port_to)});
  return links;
}

nlohmann::json make_macro_definition(GraphNode                      &graph,
                                     const std::vector<std::string> &node_ids,
                                     const std::string              &name,
                                     std::vector<PortLink>          &outer_links)
{
  const std::set<std::string> selection(node_ids.begin(), node_ids.end());
  if (selection.empty())
    throw std::invalid_argument("Select the nodes to pack into a macro.");

  nlohmann::json body;
  body["nodes"] = nlohmann::json::array();
  body["links"] = nlohmann::json::array();

  for (const auto &id : selection)
  {
    auto *node = graph.get_node_ref_by_id<BaseNode>(id);
    if (!node)
      throw std::invalid_argument("Unknown node " + id);
    // a tag per instance would collide, and the macro would not know its graph
    if (node->get_node_type() == "Broadcast")
      throw std::invalid_argument("Broadcast nodes can't go into a macro.");
    body["nodes"].push_back(node->json_to());
  }

  // one macro port per output crossing the selection boundary
  struct Crossing
  {
    std::string           node, port, data_type;
    std::vector<PortLink> links;
  };
  std::vector<Crossing> inputs, outputs;

  auto crossing = [](std::vector<Crossing> &list,
                     const std::string     &node,
                     const std::string     &port) -> Crossing &
  {
    for (auto &c : list)
      if (c.node == node && c.port == port)
        return c;
    return list.emplace_back(Crossing{node, port, "", {}});
  };

  for (const PortLink &link : links_of(graph))
  {
    const bool from_in = selection.contains(link.node_out);
    const bool to_in = selection.contains(link.node_in);

    if (from_in && to_in)
      body["links"].push_back(link_json(link));
    else if (from_in || to_in)
    {
      auto        &c = crossing(to_in ? inputs : outputs, link.node_out, link.port_out);
      gnode::Node *from = graph.get_node(link.node_out);
      c.data_type = from->get_data_type(from->get_port_index(link.port_out));
      c.links.push_back(link);
    }
  }

  // interface nodes take ids after every id of the graph, so none collide
  uint                  next_id = graph.get_id_count();
  std::set<std::string> used;

  auto add_io = [&](const Crossing &c, bool is_input, int order)
  {
    const std::string suffix = suffix_of(c.data_type);
    if (suffix.empty())
      throw std::invalid_argument("A link of this type can't cross into a macro.");

    auto  p_io = graph.create_node((is_input ? "MacroInput" : "MacroOutput") + suffix);
    auto *io = static_cast<BaseNode *>(p_io.get());
    io->set_id(std::to_string(next_id++));

    // named after the port it replaces: "mask", "input", "output"...
    const std::string port_name = unique_name(is_input ? c.links.front().port_in : c.port,
                                              used);
    io->set_value<std::string>("name", port_name);
    io->set_value<int>("order", order);
    body["nodes"].push_back(io->json_to());

    // inside, the interface node takes the outside end of the links
    for (const auto &l : c.links)
      if (is_input)
        body["links"].push_back(
            link_json({io->get_id(), "output", l.node_in, l.port_in}));
      else
        outer_links.push_back({"", port_name, l.node_in, l.port_in});

    if (is_input)
      outer_links.push_back({c.node, c.port, "", port_name});
    else
      body["links"].push_back(link_json({c.node, c.port, io->get_id(), "input"}));
  };

  for (size_t k = 0; k < inputs.size(); ++k)
    add_io(inputs[k], true, static_cast<int>(k));
  for (size_t k = 0; k < outputs.size(); ++k)
    add_io(outputs[k], false, static_cast<int>(k));

  body["id_count"] = next_id;

  return {{"format", 1},
          {"macro_id", new_macro_id()},
          {"name", name},
          {"revision", 0},
          {"graph", body}};
}

} // namespace hesiod
