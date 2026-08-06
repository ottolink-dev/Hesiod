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

#include "meta/core/container_group.hpp"

#include "hesiod/model/graph/graph_config.hpp"
#include "hesiod/model/nodes/compat/legacy_compat.hpp"
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

  // Backend-agnostic, normalized attribute snapshot used by the Meta-migration
  // parity tooling. Both the legacy (attr map) and Meta (container group)
  // backends are folded into ONE identical record shape so a node flipped from
  // legacy to Meta diffs to zero against its captured legacy reference.
  nlohmann::json attribute_parity_record() const;

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
    static_assert(hsd::compat::CompatTag<T>, "add_attr<T>: T is not a compat tag");
    auto &a = hsd::compat::legacy_traits<T>::create(this->meta_group().current(),
                                                    key,
                                                    std::forward<Args>(args)...);
    this->legacy_decoders_[key] = [&a, key](const nlohmann::json &j)
    { hsd::compat::legacy_traits<T>::decode(a, j, key); };
  }

  template <typename T> auto get_attr(const std::string &key) const -> decltype(auto)
  {
    static_assert(hsd::compat::CompatTag<T>);
    using traits = hsd::compat::legacy_traits<T>;
    return traits::to_legacy(
        this->meta_group().current().value<typename traits::storage>(key));
  }

  template <typename T> auto get_attr_ref(const std::string &key) const
  {
    using storage = typename hsd::compat::legacy_traits<T>::storage;
    // legacy get_attr_ref was const-returning-mutable; mirror that
    auto &c = const_cast<BaseNode *>(this)->meta_group().current();
    auto *p = c.find(key);
    if (!p)
      throw std::invalid_argument("unknown attribute key: " + key);
    auto *typed = p->template try_cast<meta::Attribute<storage>>();
    if (!typed)
      throw std::runtime_error("wrong attribute type for key: " + key);
    return typename hsd::compat::handle_of<T>::type(typed);
  }

  // Native-Meta nodes (no compat tag) can register a hand-written decoder
  // for a key written by the legacy Attributes serializer, so old .hsd
  // files keep loading after the node stops using add_attr<LegacyType>.
  void register_legacy_decoder(const std::string                          &key,
                               std::function<void(const nlohmann::json &)> fn)
  {
    this->legacy_decoders_[key] = std::move(fn);
  }

  std::vector<std::string> *get_attr_ordered_key_ref();
  void set_attr_ordered_key(const std::vector<std::string> &new_attr_ordered_key);

  meta::ContainerGroup       &meta_group(); // lazily creates group + "main" container
  const meta::ContainerGroup &meta_group() const;

  void                  finalize_attributes();
  const nlohmann::json &initial_meta_state() const { return this->initial_meta_state_; }

  void reseed(bool backward);

  // --- Callbacks - "signals" equivalent
  std::function<void(const std::string &id)> compute_finished;
  std::function<void(const std::string &id)> compute_started;

private:
  // --- Members ---
  std::unique_ptr<meta::ContainerGroup>
      meta_group_; // opt-in Meta storage (nullptr = legacy attr map)

  // legacy-json fallback decoders, registered by add_attr (compat tags only)
  std::map<std::string, std::function<void(const nlohmann::json &)>> legacy_decoders_;
  // container state captured at finalize time; toolbar "Reset Settings" restores it
  nlohmann::json initial_meta_state_;

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