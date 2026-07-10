/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <array>
#include <chrono>
#include <functional>
#include <stdexcept>
#include <tuple>
#include <utility>

#include "gnode/node.hpp"
#include "gnodegui/node_proxy.hpp"

#include "attributes/abstract_attribute.hpp"
#include "meta/core/container_group.hpp"

#include "hesiod/model/graph/graph_config.hpp"
#include "hesiod/model/nodes/node_runtime_info.hpp"

// clang-format off
#define CONFIG(obj) obj.get_config_ref()->shape, obj.get_config_ref()->tile_shape, obj.get_config_ref()->halo, obj.get_config_ref()->storage_mode
#define CONFIG_TEX(obj) obj.get_config_ref()->shape, obj.get_config_ref()->tile_shape, obj.get_config_ref()->halo, 4, obj.get_config_ref()->storage_mode
// clang-format on

namespace hesiod
{

class GraphNode; // forward

// helper
std::string map_type_name(const std::string &typeid_name);

// =====================================
// BaseNode
// =====================================
class BaseNode : public gnode::Node, public std::enable_shared_from_this<BaseNode>
{
public:
  // --- Constructors ---
  BaseNode() = default;
  BaseNode(const std::string &label, std::weak_ptr<GraphConfig> config);

  std::shared_ptr<BaseNode> get_shared();

  // --- Configuration ---
  const GraphConfig                 &cfg() const;
  std::shared_ptr<const GraphConfig> get_config_ref() const;
  void                               propagate_config_change();

  // --- Runtime info ---
  NodeRuntimeInfo get_runtime_info() const;
  float           get_memory_usage() const;
  void            update_runtime_info(NodeRuntimeStep step);

  // --- Identification ---
  std::string get_id() const;
  void        set_id(const std::string &new_id);
  std::string get_category() const;
  void        set_comment(const std::string &new_comment);
  std::string get_node_type() const;

  // --- Compute ---
  void compute() override;
  void set_compute_fct(std::function<void(BaseNode &node)> new_compute_fct);

  // --- Serialization ---
  virtual void           json_from(nlohmann::json const &json);
  virtual nlohmann::json json_to() const;
  nlohmann::json         node_parameters_to_json() const;

  // --- Documentation ---
  nlohmann::json get_documentation() const;
  std::string    get_documentation_html() const;
  std::string    get_documentation_short() const;
  std::string    get_documentation_short_html() const;
  void           update_attributes_tool_tip();

  // --- Proxy (most of it) ---
  std::string     get_caption() const;
  std::string     get_comment() const;
  void           *get_data_ref(int port_index);
  std::string     get_data_type(int port_index) const;
  int             get_nports() const;
  std::string     get_port_caption(int port_index) const;
  gngui::PortType get_port_type(int port_index) const;
  std::string     get_tool_tip_text();

  // --- Attribute Management ---
  template <typename T, typename... Args>
  void add_attr(const std::string &key, Args &&...args)
  {
    this->attr[key] = std::make_unique<T>(std::forward<Args>(args)...);
  }

  template <typename T> auto get_attr(const std::string &key) const -> decltype(auto)
  {
    if (!this->attr.contains(key))
      std::invalid_argument("unknown attribute key: " + key);
    return this->attr.at(key)->get_ref<T>()->get_value();
  }

  template <typename T> T *get_attr_ref(const std::string &key) const
  {
    return this->attr.at(key)->get_ref<T>();
  }

  std::vector<std::string> *get_attr_ordered_key_ref();
  std::map<std::string, std::unique_ptr<attr::AbstractAttribute>> *get_attributes_ref();
  void set_attr_ordered_key(const std::vector<std::string> &new_attr_ordered_key);

  bool                        uses_meta() const;
  meta::ContainerGroup       &meta_group();       // lazily creates group + "main" container
  const meta::ContainerGroup &meta_group() const;

  void reseed(bool backward);

  // --- Callbacks - "signals" equivalent
  std::function<void(const std::string &id)> compute_finished;
  std::function<void(const std::string &id)> compute_started;

private:
  // --- Members ---
  std::map<std::string, std::unique_ptr<attr::AbstractAttribute>> attr = {};
  std::unique_ptr<meta::ContainerGroup> meta_group_; // opt-in Meta storage (nullptr = legacy attr map)

  std::vector<std::string>            attr_ordered_key = {};
  std::string                         category;
  std::string                         comment;
  std::weak_ptr<GraphConfig>          config; // owned by GraphNode
  nlohmann::json                      documentation;
  NodeRuntimeInfo                     runtime_info;
  std::function<void(BaseNode &node)> compute_fct = nullptr;
};

// =====================================
// Node-related enums
// =====================================
enum BlendingMethod : int
{
  ADD,
  EXCLUSION_BLEND,
  GRADIENTS,
  MAXIMUM,
  MAXIMUM_SMOOTH,
  MINIMUM,
  MINIMUM_SMOOTH,
  MULTIPLY,
  MULTIPLY_ADD,
  NEGATE,
  OVERLAY,
  REPLACE,
  SOFT,
  SUBSTRACT,
};

enum ExportFormat : int
{
  PNG8BIT,
  PNG16BIT,
  RAW16BIT,
  EXR32BIT,
};

enum MaskCombineMethod : int
{
  UNION,
  INTERSECTION,
  EXCLUSION,
};

// --- helpers

void setup_background_image_for_cloud_attribute(BaseNode          &node,
                                                const std::string &attribute_key,
                                                const std::string &port_id);

void setup_histogram_for_range_attribute(BaseNode          &node,
                                         const std::string &attribute_key,
                                         const std::string &port_id);

// unpack vectors
template <std::size_t N, typename T, std::size_t... Is>
auto unpack_impl(const std::vector<T *> &v, std::index_sequence<Is...>)
{
  assert(v.size() >= N);
  return std::make_tuple(v[Is]...);
}

template <std::size_t N, typename T> auto unpack(const std::vector<T *> &v)
{
  return unpack_impl<N>(v, std::make_index_sequence<N>{});
}

} // namespace hesiod