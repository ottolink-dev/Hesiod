/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <any>
#include <array>
#include <chrono>
#include <functional>
#include <map>
#include <stdexcept>
#include <tuple>
#include <utility>

#include "gnode/node.hpp"
#include "gnodegui/node_proxy.hpp"

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

  // --- Meta Accessors & Helpers ---
  template <typename T> decltype(auto) val(const std::string &key) const
  {
    return this->get_meta_group().current().value<T>(key);
  }

  template <typename T> void set_value(const std::string &key, T new_value)
  {
    this->get_meta_group().current().value<T>(key) = new_value;
  }

  template <typename T>
  decltype(auto) metadata_val(const std::string &key, const std::string &meta_key) const
  {
    const meta::AbstractAttribute *handle = this->get_meta_group().current().find(key);
    return handle->metadata().value<T>(meta_key);
  }

  template <typename T>
  void set_metadata(const std::string &key, const std::string &meta_key, T new_value)
  {
    meta::AbstractAttribute *handle = this->get_meta_group().current().find(key);
    handle->metadata().value<T>(meta_key) = new_value;
  }

  template <typename T>
  decltype(auto) state_val(const std::string &key, const std::string &meta_key) const
  {
    const meta::AbstractAttribute *handle = this->get_meta_group().current().find(key);
    return handle->state().value<T>(meta_key);
  }

  template <typename T>
  void set_state(const std::string &key, const std::string &meta_key, T new_value)
  {
    meta::AbstractAttribute *handle = this->get_meta_group().current().find(key);
    handle->state().value<T>(meta_key) = new_value;
  }

  template <typename T> meta::Attribute<T> *attr(const std::string &key)
  {
    auto *p = this->get_meta_group().current().find(key);
    if (!p)
      return nullptr;
    return p->template try_cast<meta::Attribute<T>>();
  }

  meta::ContainerGroup       &get_meta_group(); // lazily creates group + "main" container
  const meta::ContainerGroup &get_meta_group() const;
  void                        set_current_group(const std::string &group_name);
  void                        set_current_category(const std::string &category);
  const std::string &get_current_category() const { return this->current_category; }

  void                  finalize_attributes();
  const nlohmann::json &get_initial_meta_state() const
  {
    return this->initial_meta_state;
  }

  /** @brief Default value of an attribute, or an empty any if there is none.
   *
   * Taken straight off the attribute at finalize time, so it covers every type
   * the node can hold rather than the handful that survive a round trip
   * through json. The properties panel uses it to decide what counts as
   * modified.
   */
  std::any get_initial_default(const std::string &container_name,
                               const std::string &key) const;

  void reseed(bool backward);

  // --- Callbacks - "signals" equivalent
  std::function<void(const std::string &id)> compute_finished;
  std::function<void(const std::string &id)> compute_started;

private:
  // --- Members ---
  std::unique_ptr<meta::ContainerGroup> meta_group; // attribute storage
  std::string                           current_category;

  // container state captured at finalize time; toolbar "Reset Settings" restores it
  nlohmann::json initial_meta_state;

  // the same snapshot as live values, keyed by container then attribute name
  std::map<std::string, std::map<std::string, std::any>> initial_meta_defaults;

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
