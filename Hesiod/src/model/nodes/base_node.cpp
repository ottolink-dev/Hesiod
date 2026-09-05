/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <format>
#include <fstream>
#include <new>
#include <optional>
#include <unordered_map>

#include <QCoreApplication>

#include "highmap/geometry/cloud.hpp"
#include "highmap/geometry/path.hpp"
#include "highmap/virtual_array/virtual_array.hpp"

#include "meta/ext/array/array.hpp"
#include "meta/metadata/keys.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/legacy/legacy_converter.hpp"
#include "hesiod/model/nodes/node_factory.hpp"
#include "hesiod/model/utils.hpp"

namespace hesiod
{

// --- helper

std::string map_type_name(const std::string &typeid_name)
{
  static const std::unordered_map<std::string, std::string> type_name_map = {
      {typeid(hmap::Array).name(), "Array"},
      {typeid(hmap::Cloud).name(), "Cloud"},
      {typeid(hmap::VirtualArray).name(), "VirtualArray"},
      {typeid(hmap::VirtualTexture).name(), "VirtualTexture"},
      {typeid(hmap::Path).name(), "Path"},
      {typeid(std::vector<float>).name(), "vector<float>"}};

  auto it = type_name_map.find(typeid_name);
  return (it != type_name_map.end()) ? it->second : typeid_name;
}

// --- class definition

BaseNode::BaseNode(const std::string &label, std::weak_ptr<GraphConfig> config)
    : gnode::Node(label), config(config)
{
  Logger::log()->trace("BaseNode::BaseNode, label: {}", label);

  this->category = get_node_inventory().at(label);
  this->update_runtime_info(NodeRuntimeStep::NRS_INIT);

  // initialize documentation
  const nlohmann::json &json = HSD_CTX.node_documentation;

  // safely load documentation
  if (json.contains(label) && json[label].is_object())
  {
    this->documentation = json[label];
  }
  else
  {
    Logger::log()->warn("Missing documentation for node: {}", label);
    this->documentation = nlohmann::json::object();
  }
}

const GraphConfig &BaseNode::cfg() const
{
  auto ptr = config.lock();

  if (!ptr)
  {
    Logger::log()->critical("BaseNode::get_category: Config ptr is nullptr, node: {}/{}",
                            this->get_caption(),
                            this->get_id());
    throw std::runtime_error("Config ptr is nullptr.");
  }

  return *ptr;
}

void BaseNode::compute()
{
  if (this->compute_started)
    this->compute_started(this->get_id());

  this->update_runtime_info(NodeRuntimeStep::NRS_UPDATE_START);

  // A node's compute can throw: out-of-memory at high resolution, an OpenCL
  // error, a bad parameter combination. Unguarded, that unwinds out of the Qt
  // event loop and terminates the process. Contain it here so one failing node
  // cannot take the application down.
  bool out_of_memory = false;

  try
  {
    this->compute_fct(*this);
  }
  catch (const std::bad_alloc &)
  {
    this->runtime_info.error_count++;
    Logger::log()->critical("BaseNode::compute: out of memory computing node '{}' "
                            "({}); abandoning this graph update - lower the "
                            "resolution and try again",
                            this->get_id(),
                            this->get_node_type());
    out_of_memory = true;
  }
  catch (const std::exception &e)
  {
    this->runtime_info.error_count++;
    Logger::log()->critical("BaseNode::compute: node '{}' ({}) failed: {}",
                            this->get_id(),
                            this->get_node_type(),
                            e.what());
  }
  catch (...)
  {
    this->runtime_info.error_count++;
    Logger::log()->critical("BaseNode::compute: node '{}' ({}) failed with an "
                            "unknown exception",
                            this->get_id(),
                            this->get_node_type());
  }

  this->update_runtime_info(NodeRuntimeStep::NRS_UPDATE_END);

  if (this->compute_finished)
    this->compute_finished(this->get_id());

  // Memory exhaustion is not a per-node problem: pressing on would thrash
  // through every remaining node and fail again. Abandon the whole update, but
  // only after this node's own bookkeeping has run, so the UI is not left
  // showing it as still computing.
  if (out_of_memory)
    throw std::bad_alloc();
}

meta::ContainerGroup &BaseNode::get_meta_group()
{
  if (!this->meta_group)
  {
    this->meta_group = std::make_unique<meta::ContainerGroup>();
    this->meta_group->add("main");
    this->meta_group->set_current("main");
  }
  return *this->meta_group;
}

const meta::ContainerGroup &BaseNode::get_meta_group() const { return *this->meta_group; }

void BaseNode::set_current_group(const std::string &group_name)
{
  auto &group = this->get_meta_group();

  // Erase any existing empty containers that are not the target group
  std::vector<std::string> to_erase;
  for (const auto &name : group.insertion_order())
  {
    if (name != group_name)
    {
      const auto *c = group.find(name);
      if (c && c->size() == 0)
      {
        to_erase.push_back(name);
      }
    }
  }

  for (const auto &name : to_erase)
  {
    group.erase(name);
  }

  if (!group.find(group_name))
    group.add(group_name);

  group.set_current(group_name);
}

void BaseNode::set_current_category(const std::string &category)
{
  this->current_category = category;
}

std::string BaseNode::get_category() const { return this->category; }

std::shared_ptr<const GraphConfig> BaseNode::get_config_ref() const
{
  auto ptr = config.lock();

  if (!ptr)
  {
    Logger::log()->critical("BaseNode::get_category: Config ptr is nullptr, node: {}/{}",
                            this->get_caption(),
                            this->get_id());
    throw std::runtime_error("Config ptr is nullptr.");
  }

  return ptr;
}

std::string BaseNode::get_documentation_html() const
{
  std::string html;

  try
  {
    html += std::format("<h1>{} Node</h1>", this->get_label());

    // category and description
    if (this->documentation.contains("category"))
    {
      html += std::format("<p><b>Categories: {}</b></p>",
                          this->documentation["category"].get<std::string>());
    }

    html += std::format(
        "<p>{}</p>",
        this->documentation.value("description", "No description available"));

    // ports table
    html += "<h2>Ports</h2>";
    html += "<table border='1' cellspacing='0' cellpadding='5'>"
            "<tr><th>Name</th><th>I/O</th><th>Data Type</th><th>Description</th></tr>";

    if (this->documentation.contains("ports") && this->documentation["ports"].is_object())
    {
      for (const auto &[key, port] : this->documentation["ports"].items())
      {
        html += std::format(
            "<tr><td><b>{}</b></td><td>{}</td><td>{}</td><td>{}</td></tr>",
            port.value("caption", key),
            port.value("type", "Unknown"),
            port.value("data_type", "Unknown"),
            port.value("description", "No description"));
      }
    }
    html += "</table>";

    // parameters table
    html += "<h2>Parameters</h2>";
    html += "<table border='1' cellspacing='0' cellpadding='5'>"
            "<tr><th>Name</th><th>Type</th><th>Description</th></tr>";

    if (this->documentation.contains("parameters") &&
        this->documentation["parameters"].is_object())
    {
      for (const auto &[key, param] : this->documentation["parameters"].items())
      {
        html += std::format("<tr><td><b>{}</b></td><td>{}</td><td>{}</td></tr>",
                            param.value("label", key),
                            param.value("type", "Unknown"),
                            param.value("description", "No description"));
      }
    }
    html += "</table>";
  }
  catch (const std::exception &e)
  {
    Logger::log()->error(
        "BaseNode::get_documentation_html: Error generating documentation HTML: {}",
        e.what());
    html = "<p>Error generating documentation</p>";
  }

  return html;
}

nlohmann::json BaseNode::get_documentation() const { return this->documentation; }

std::string BaseNode::get_documentation_short() const
{
  std::string str;
  size_t      width = 64;

  try
  {
    str += std::format("NODE TYPE: {}", this->get_label());
    str += "\n\n";

    // category
    if (this->documentation.contains("category"))
    {
      str += std::format("CATEGORIES: {}",
                         this->documentation["category"].get<std::string>());
      str += "\n\n";
    }

    // description
    std::string description = this->documentation.value("description",
                                                        "No description available");
    description = wrap_text(description, width);
    str += "DESCRIPTION:\n" + description;
    str += "\n\n";

    // ports
    if (this->documentation.contains("ports") && this->documentation["ports"].is_object())
    {
      str += "PORTS:\n";

      for (const auto &[key, port] : this->documentation["ports"].items())
      {
        std::string port_description = port.value("description", "No description");
        port_description = wrap_text(port_description, width);

        str += std::format("- {} / {} / {}\n{}\n",
                           port.value("caption", key),
                           port.value("type", "Unknown"),
                           port.value("data_type", "Unknown"),
                           port_description);
      }
    }
  }
  catch (const std::exception &e)
  {
    Logger::log()->error(
        "BaseNode::get_documentation_short: Error generating documentation HTML: {}",
        e.what());
    str = "Error generating documentation";
  }

  return str;
}

std::string BaseNode::get_documentation_short_html() const
{
  std::string html = "<div><font size=\"-1\">";

  std::string font_color_tag = std::format(
      "<font color='{}'>",
      HSD_CTX.app_settings.colors.text_secondary.name().toStdString());

  try
  {
    html += "<b>" + this->get_label() + "</b><br>";

    // category and description
    if (this->documentation.contains("category"))
    {
      html += "<i>" + this->documentation["category"].get<std::string>() + "</i>";
    }

    // main description
    html += font_color_tag;
    html += std::format(
        "<p>{}</p>",
        this->documentation.value("description", "No description available"));
    html += "</font>";

    // ports
    if (this->documentation.contains("ports") && this->documentation["ports"].is_object())
    {
      // html += "Ports";
      html += font_color_tag;
      html += "<ul>";

      for (const auto &[key, port] : this->documentation["ports"].items())
      {
        const std::string caption = port.value("caption", key);
        const std::string type = port.value("type", "Unknown");
        const std::string dtype = port.value("data_type", "Unknown");
        const std::string description = port.value("description", "No description");

        html += "<li>";
        html += "<b>" + caption + "</b>";
        html += " &mdash; ";
        html += "<i>" + type + "</i>";
        html += " (" + dtype + ")<br>";
        html += description;
        html += "</li>";
      }
      html += "</ul></font>";
    }

    html += "</div>";
  }
  catch (const std::exception &e)
  {
    Logger::log()->error(
        "BaseNode::get_documentation_html: Error generating documentation HTML: {}",
        e.what());
    html = "<p>Error generating documentation</p>";
  }

  return html;
}

std::string BaseNode::get_id() const { return gnode::Node::get_id(); }

float BaseNode::get_memory_usage() const
{
  // only count big float arrays
  size_t unit = size_t(this->get_config_ref()->shape.x *
                       this->get_config_ref()->shape.y) *
                sizeof(float);
  size_t count = 0;

  for (int k = 0; k < this->get_nports(); k++)
  {
    // only outputs carry data
    if (this->get_port_type(k) == gngui::PortType::IN)
      continue;

    if (this->get_data_type(k) == typeid(hmap::VirtualArray).name())
    {
      count += unit;
    }
    else if (this->get_data_type(k) == typeid(hmap::VirtualTexture).name())
    {
      count += 4.f * unit;
    }
    else if (this->get_data_type(k) == typeid(hmap::Array).name())
    {
      count += unit;
    }
    else if (this->get_data_type(k) == typeid(std::vector<float>).name())
    {
      auto *p_d = this->get_value_ref<std::vector<float>>(k);
      if (p_d)
        count += sizeof(float) * p_d->size();
    }
    else if (this->get_data_type(k) == typeid(hmap::Cloud).name())
    {
      auto *p_d = this->get_value_ref<hmap::Cloud>(k);
      if (p_d)
        count += sizeof(float) * 3 * p_d->size();
    }
    else if (this->get_data_type(k) == typeid(hmap::Path).name())
    {
      auto *p_d = this->get_value_ref<hmap::Path>(k);
      if (p_d)
        count += sizeof(float) * 3 * p_d->size();
    }
  }

  if (count > 0.)
    return (float)(count) / 1048576.f; // in MB
  else
    return -1.f; // to signal not implemented types
}

std::string BaseNode::get_node_type() const { return this->get_label(); }

NodeRuntimeInfo BaseNode::get_runtime_info() const { return this->runtime_info; }

std::shared_ptr<BaseNode> BaseNode::get_shared()
{
  try
  {
    return shared_from_this();
  }
  catch (...)
  {
    Logger::log()->critical("BaseNode::get_shared: object is not managed by shared_ptr");
    return nullptr;
  }
}

void BaseNode::finalize_attributes()
{
  auto &group = this->get_meta_group();

  // initial state for toolbar Reset and sync common attributes accros
  // group containers
  this->initial_meta_state = group.json_to(meta::SerializationMode::state);

  // The same snapshot kept as live values. The panel needs defaults to decide
  // what is modified, and reading them back out of the json only ever worked
  // for the types json round-trips cleanly, which silently left every other
  // row looking untouched.
  this->initial_meta_defaults.clear();

  for (const auto &[container_name, p_container] : group.containers())
  {
    if (!p_container)
      continue;

    auto &defaults = this->initial_meta_defaults[container_name];

    for (const auto &key : p_container->insertion_order())
      if (const auto *p_attr = p_container->find(key))
        defaults[key] = p_attr->to_any();
  }

  group.synchronize_all();
  group.set_current_to_first();
}

std::any BaseNode::get_initial_default(const std::string &container_name,
                                       const std::string &key) const
{
  auto container_it = this->initial_meta_defaults.find(container_name);
  if (container_it == this->initial_meta_defaults.end())
    return {};

  auto key_it = container_it->second.find(key);
  if (key_it == container_it->second.end())
    return {};

  return key_it->second;
}

void BaseNode::json_from(nlohmann::json const &json)
{
  try
  {
    this->set_id(json.value("id", this->get_id()));

    if (json.contains("comment"))
      this->set_comment(json["comment"]);

    if (json.contains("runtime_info"))
      this->runtime_info.json_from(json["runtime_info"]);

    auto          &group = this->get_meta_group();
    nlohmann::json group_json = convert_legacy_container_group_json(group, json);

    group.json_from(group_json, meta::SerializationMode::state);
  }
  catch (const nlohmann::json::exception &e)
  {
    Logger::log()->error("BaseNode::json_from: JSON parsing error: {}", e.what());
  }
}

nlohmann::json BaseNode::json_to() const
{
  nlohmann::json json;

  try
  {
    json["id"] = this->get_id();
    json["label"] = this->get_label();
    json["comment"] = this->get_comment();
    json["runtime_info"] = this->runtime_info.json_to();

    auto meta_json = this->get_meta_group().json_to(meta::SerializationMode::state);
    for (auto &[key, val] : meta_json.items())
    {
      json[key] = val;
    }
  }
  catch (const std::exception &e)
  {
    Logger::log()->error("BaseNode::json_to: Error serializing node to JSON: {}",
                         e.what());
  }
  return json;
}

nlohmann::json BaseNode::node_parameters_to_json() const
{
  nlohmann::json json;

  try
  {
    // Basic node info
    json["label"] = this->get_label();
    json["category"] = this->category;
    json["description"] = this->documentation.value("description",
                                                    "No description available");

    // Port information
    nlohmann::json ports_json;
    for (int k = 0; k < this->get_nports(); k++)
    {
      nlohmann::json    port_info;
      const std::string caption = this->get_port_caption(k);

      port_info["type"] = (this->get_port_type(k) == gngui::PortType::IN) ? "input"
                                                                          : "output";
      port_info["caption"] = caption;
      port_info["data_type"] = map_type_name(this->get_data_type(k));

      auto json_ptr = nlohmann::json::json_pointer("/ports/" + caption + "/description");
      port_info["description"] = this->documentation.value(json_ptr, "No description");

      ports_json[caption] = port_info;
    }
    json["ports"] = ports_json;

    // Attribute information: walk every container of the meta group, not just
    // the current one, so grouped nodes document the parameters of all their
    // groups. "parameters" stays a flat key->info map (tooltip lookups address
    // it by key); "parameter_groups" preserves the group structure and order.
    nlohmann::json params_json;
    nlohmann::json groups_json = nlohmann::json::array();

    const auto &group = this->get_meta_group();

    for (const auto &group_name : group.insertion_order())
    {
      const auto *container = group.find(group_name);
      if (!container)
        continue;

      nlohmann::json group_info;
      group_info["name"] = group_name;
      std::vector<std::string> group_keys;

      for (const auto &key : container->insertion_order())
      {
        const auto *p = container->find(key);
        if (!p)
          continue;

        group_keys.push_back(key);

        // an attribute shared between groups is documented once
        if (params_json.contains(key))
          continue;

        nlohmann::json param_info;
        param_info["key"] = key;
        const std::string *lbl = p->metadata().try_value<std::string>(
            meta::keys::ui::label);
        param_info["label"] = lbl ? *lbl : key;
        const std::string *doc_type = p->metadata().try_value<std::string>(
            ATTR_DOC_TYPE_KEY);
        param_info["type"] = doc_type ? *doc_type : "";
        auto json_ptr = nlohmann::json::json_pointer("/parameters/" + key +
                                                     "/description");
        param_info["description"] = this->documentation.value(json_ptr, "No description");
        params_json[key] = param_info;
      }

      group_info["keys"] = group_keys;
      groups_json.push_back(group_info);
    }

    json["parameters"] = params_json;
    json["parameter_groups"] = groups_json;
  }
  catch (const std::exception &e)
  {
    Logger::log()->error(
        " BaseNode::node_parameters_to_json: Error generating node parameters JSON: {}",
        e.what());
  }

  return json;
}

void BaseNode::propagate_config_change()
{
  Logger::log()->trace("BaseNode::propagate_config_change: node {}/{}",
                       this->get_caption(),
                       this->get_id());

  const GraphConfig &cfg = *this->get_config_ref();

  // go through the data and modify is needed (only outputs hold data)
  for (int k = 0; k < this->get_nports(); k++)
    if (this->get_port_type(k) == gngui::PortType::OUT)
    {
      const std::string type = this->get_data_type(k);

      if (type == typeid(hmap::VirtualTexture).name())
      {
        auto *p_v = this->get_value_ref<hmap::VirtualTexture>(k);
        if (p_v)
          *p_v = hmap::VirtualTexture(cfg.shape,
                                      cfg.tile_shape,
                                      cfg.halo,
                                      4, // RGBA
                                      cfg.storage_mode);
      }
      else if (type == typeid(hmap::Array).name())
      {
        auto *p_v = this->get_value_ref<hmap::Array>(k);
        if (p_v)
          *p_v = hmap::Array(cfg.shape);
      }
      else if (type == typeid(hmap::VirtualArray).name())
      {
        auto *p_v = this->get_value_ref<hmap::VirtualArray>(k);
        if (p_v)
        {
          auto va = hmap::VirtualArray(cfg.shape,
                                       cfg.tile_shape,
                                       cfg.halo,
                                       cfg.storage_mode);
          p_v->copy_from(va, cfg.cm_cpu);
        }
      }
    }

  // Update shape for the UI of meta::Array attributes
  for (const auto &key : this->get_meta_group().current().insertion_order())
  {
    auto *p = this->get_meta_group().current().find(key);

    if (!p)
      continue;

    if (auto *a = p->try_cast<meta::Attribute<meta::Array>>())
    {
      if (auto *width_ptr = a->metadata().try_value<int>(meta::keys::ui::width))
        *width_ptr = cfg.shape.x;
      else
        a->metadata().try_add(meta::keys::ui::width, cfg.shape.x);

      if (auto *height_ptr = a->metadata().try_value<int>(meta::keys::ui::height))
        *height_ptr = cfg.shape.y;
      else
        a->metadata().try_add(meta::keys::ui::height, cfg.shape.y);
    }
  }
}

void BaseNode::reseed(bool backward)
{
  for (const auto &key : this->get_meta_group().current().insertion_order())
  {
    auto *p = this->get_meta_group().current().find(key);
    if (!p)
      continue;
    if (key == "seed")
      if (auto *typed = p->try_cast<meta::Attribute<int>>())
      {
        int increment = backward ? -1 : 1;
        typed->set_from_any(typed->value() + increment);
      }
  }
}

void BaseNode::set_comment(const std::string &new_comment)
{
  this->comment = new_comment;
}

void BaseNode::set_compute_fct(std::function<void(BaseNode &node)> new_compute_fct)
{
  this->compute_fct = std::move(new_compute_fct);
}

void BaseNode::set_id(const std::string &new_id) { gnode::Node::set_id(new_id); }

void BaseNode::update_attributes_tool_tip()
{
  Logger::log()->trace("BaseNode::update_attributes_tool_tip");

  size_t width = 64;

  for (const auto &[container_name, p_container] : this->get_meta_group().containers())
  {
    if (!p_container)
      continue;

    for (const auto &key : p_container->insertion_order())
    {
      auto *p = p_container->find(key);
      if (!p)
        continue;

      const std::string *lbl = p->metadata().try_value<std::string>(
          meta::keys::ui::label);
      std::string label = lbl ? *lbl : key;

      if (this->documentation.contains("parameters") &&
          this->documentation["parameters"].contains(key))
      {
        std::string description = "<div><font size=\"+1\"><b>" +
                                  remove_trailing_char(label, ':') + "</font></b><br>";

        description += "<font color='COLOR_TEXT_SECONDARY'>";

        replace_all(description,
                    "COLOR_TEXT_SECONDARY",
                    HSD_CTX.app_settings.colors.text_secondary.name().toStdString());

        if (this->documentation["parameters"][key].contains("description"))
        {
          std::string base_desc = this->documentation["parameters"][key]["description"];
          base_desc = wrap_text(base_desc, width);
          description += base_desc;
        }

        description += "</div>";

        p->metadata()
            .try_add(std::string(meta::keys::ui::tooltip), std::string(description))
            ->value() = description;
      }
    }
  }
}

void BaseNode::update_runtime_info(NodeRuntimeStep step)
{
  // TODO move this method to NodeRuntimeInfo class?

  switch (step)
  {
  case NodeRuntimeStep::NRS_INIT:
  {
    this->runtime_info.time_creation = std::chrono::system_clock::now();
  }
  break;

  case NodeRuntimeStep::NRS_UPDATE_START:
  {
    this->runtime_info.timer_t0 = std::chrono::steady_clock::now();
  }
  break;

  case NodeRuntimeStep::NRS_UPDATE_END:
  {
    this->runtime_info.time_last_update = std::chrono::system_clock::now();
    this->runtime_info.eval_count++;

    // elapsed
    auto t1 = std::chrono::steady_clock::now();
    this->runtime_info
        .update_time = (float)std::chrono::duration_cast<std::chrono::nanoseconds>(
                           t1 - this->runtime_info.timer_t0)
                           .count() *
                       1e-6f;
  }
  break;

  default:
    return;
  }
}

} // namespace hesiod
