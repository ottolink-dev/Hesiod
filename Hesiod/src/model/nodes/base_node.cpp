/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <format>
#include <fstream>
#include <unordered_map>

#include <QCoreApplication>

#include "highmap/geometry/cloud.hpp"
#include "highmap/geometry/path.hpp"
#include "highmap/virtual_array/virtual_array.hpp"

#include "attributes/seed_attribute.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
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
    this->compute_finished(this->get_id());

  this->update_runtime_info(NodeRuntimeStep::NRS_UPDATE_START);

  this->compute_fct(*this);

  this->update_runtime_info(NodeRuntimeStep::NRS_UPDATE_END);

  if (this->compute_finished)
    this->compute_finished(this->get_id());
}

std::map<std::string, std::unique_ptr<attr::AbstractAttribute>> *BaseNode::
    get_attributes_ref()
{
  return &this->attr;
};

std::vector<std::string> *BaseNode::get_attr_ordered_key_ref()
{
  return &this->attr_ordered_key;
};

bool BaseNode::uses_meta() const { return this->meta_group_ != nullptr; }

meta::ContainerGroup &BaseNode::meta_group()
{
  if (!this->meta_group_)
  {
    this->meta_group_ = std::make_unique<meta::ContainerGroup>();
    this->meta_group_->add("main");
    this->meta_group_->set_current("main");
  }
  return *this->meta_group_;
}

const meta::ContainerGroup &BaseNode::meta_group() const { return *this->meta_group_; }

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
  if (!this->uses_meta())
    return;

  auto &c = this->meta_group().current();

  // 1) _GROUPBOX_ sentinels in the ordered-key list -> ui.category metadata
  //    + build the sanitized display order
  std::vector<std::string> order;
  std::string              category = "";

  for (const auto &key : this->attr_ordered_key)
  {
    if (key.starts_with("_GROUPBOX_BEGIN_"))
    {
      category = key.substr(std::string("_GROUPBOX_BEGIN_").size());
      continue;
    }
    if (key.starts_with("_GROUPBOX_END"))
    {
      category = "";
      continue;
    }
    auto *p = c.find(key);
    if (!p)
    {
      Logger::log()->warn("finalize_attributes: node {}: ordered key '{}' not found",
                          this->get_label(),
                          key);
      continue;
    }
    if (!category.empty())
      p->metadata().try_add(std::string(meta::keys::ui::category),
                            std::string(category))
          ->value() = category;
    order.push_back(key);
  }

  // 2) render order = legacy ordered-key order, unlisted keys appended
  if (!order.empty())
  {
    for (const auto &key : c.insertion_order())
      if (std::find(order.begin(), order.end(), key) == order.end())
        order.push_back(key);
    if (!c.set_insertion_order(order))
      Logger::log()->warn("finalize_attributes: node {}: set_insertion_order rejected",
                          this->get_label());
  }

  // 3) initial state for toolbar Reset
  this->initial_meta_state_ = c.json_to();
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

    for (auto &[key, attr] : this->attr)
    {
      if (json.contains(key))
        attr->json_from(json[key]);
      else
        Logger::log()->warn("Missing JSON key for attribute: {}, using default", key);
    }

    if (this->uses_meta())
    {
      if (json.contains("_meta"))
      {
        this->meta_group().current().json_from(json["_meta"]);
      }
      else if (!this->legacy_decoders_.empty())
      {
        // legacy-format file: decode per-key values written by the old
        // Attributes library into the Meta container
        Logger::log()->info(
            "BaseNode::json_from: node '{}' loading legacy-format parameters",
            this->get_id());
        for (const auto &[key, decoder] : this->legacy_decoders_)
        {
          if (json.contains(key))
            decoder(json[key]);
          else
            Logger::log()->warn("Missing JSON key for attribute: {}, using default", key);
        }
      }
      else
      {
        Logger::log()->error(
            "BaseNode::json_from: node '{}' uses Meta storage but neither '_meta' nor "
            "legacy decoders are available — parameters NOT restored",
            this->get_id());
      }
    }
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
    for (const auto &[key, attr] : this->attr)
    {
      json[key] = attr->json_to();
    }
    json["id"] = this->get_id();
    json["label"] = this->get_label();
    json["comment"] = this->get_comment();
    json["runtime_info"] = this->runtime_info.json_to();

    if (this->uses_meta())
      json["_meta"] = this->meta_group().current().json_to();
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

    // Attribute information
    nlohmann::json params_json;
    for (const auto &[key, attr] : this->attr)
    {
      nlohmann::json param_info;
      param_info["key"] = key;
      param_info["label"] = attr->get_label();
      param_info["type"] = attr::attribute_type_map.at(attr->get_type());

      auto json_ptr = nlohmann::json::json_pointer("/parameters/" + key + "/description");
      param_info["description"] = this->documentation.value(json_ptr, "No description");

      params_json[key] = param_info;
    }

    if (this->uses_meta())
      for (const auto &key : this->meta_group().current().insertion_order())
      {
        const auto *p = this->meta_group().current().find(key);
        if (!p)
          continue;
        nlohmann::json param_info;
        param_info["key"] = key;
        const std::string *lbl = p->metadata().try_value<std::string>(
            meta::keys::ui::label);
        param_info["label"] = lbl ? *lbl : key;
        const std::string *lt = p->metadata().try_value<std::string>(
            "compat.legacy_type");
        param_info["type"] = lt ? *lt : std::string(p->type().name());
        auto json_ptr = nlohmann::json::json_pointer("/parameters/" + key +
                                                     "/description");
        param_info["description"] = this->documentation.value(json_ptr, "No description");
        params_json[key] = param_info;
      }

    json["parameters"] = params_json;
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
}

void BaseNode::reseed(bool backward)
{
  for (const auto &[key, attr] : this->attr)
    if (attr && attr->get_type() == attr::AttributeType::SEED)
      if (auto p_seed = attr->get_ref<attr::SeedAttribute>())
      {
        Logger::log()->trace("BaseNode::reseed: reseeding node {}_{}",
                             this->get_label(),
                             this->get_id());

        int increment = backward ? -1 : 1;
        p_seed->set_value(p_seed->get_value() + increment);
      }

  if (this->uses_meta())
  {
    for (const auto &key : this->meta_group().current().insertion_order())
    {
      auto *p = this->meta_group().current().find(key);
      if (!p)
        continue;
      if (const bool *is_seed = p->metadata().try_value<bool>("compat.seed");
          is_seed && *is_seed)
        if (auto *typed = p->try_cast<meta::Attribute<int>>())
        {
          int increment = backward ? -1 : 1;
          typed->set_from_any(typed->value() + increment);
        }
    }
  }
}

void BaseNode::set_attr_ordered_key(const std::vector<std::string> &new_attr_ordered_key)
{
  this->attr_ordered_key = new_attr_ordered_key;
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

  for (auto &[key, sp_attr] : this->attr)
    if (sp_attr)
    {
      std::string label = sp_attr->get_label();

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

        sp_attr->set_description(description);
      }
    }

  if (this->uses_meta())
    for (const auto &key : this->meta_group().current().insertion_order())
    {
      auto *p = this->meta_group().current().find(key);
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
