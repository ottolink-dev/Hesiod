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
#include "hesiod/model/nodes/legacy/legacy_compat.hpp"
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

  template <typename T, typename... Args>
  void add_attr(const std::string &key, Args &&...args)
  {
    static_assert(hsd::legacy::CompatTag<T>, "add_attr<T>: T is not a compat tag");
    auto &a = hsd::legacy::legacy_traits<T>::create(this->get_meta_group().current(),
                                                    key,
                                                    std::forward<Args>(args)...);
    if (!this->current_category.empty())
    {
      a.metadata().try_add(std::string(meta::keys::ui::category),
                           std::string(this->current_category));
    }
  }

  template <typename T> auto get_attr(const std::string &key) const -> decltype(auto)
  {
    static_assert(hsd::legacy::CompatTag<T>);
    using traits = hsd::legacy::legacy_traits<T>;
    return traits::to_legacy(
        this->get_meta_group().current().value<typename traits::storage>(key));
  }

  template <typename T> auto get_attr_ref(const std::string &key) const
  {
    using storage = typename hsd::legacy::legacy_traits<T>::storage;
    // legacy get_attr_ref was const-returning-mutable; mirror that
    auto &c = const_cast<BaseNode *>(this)->get_meta_group().current();
    auto *p = c.find(key);
    if (!p)
      throw std::invalid_argument("unknown attribute key: " + key);
    auto *typed = p->template try_cast<meta::Attribute<storage>>();
    if (!typed)
      throw std::runtime_error("wrong attribute type for key: " + key);
    return typename hsd::legacy::handle_of<T>::type(typed);
  }

  std::vector<std::string> *get_attr_ordered_key_ref();
  void set_attr_ordered_key(const std::vector<std::string> &new_attr_ordered_key);

  // --- Native Meta Accessors & Helpers ---
  template <typename T> decltype(auto) val(const std::string &key) const
  {
    return this->get_meta_group().current().value<T>(key);
  }

  template <typename T> meta::Attribute<T> *attr(const std::string &key)
  {
    auto *p = this->get_meta_group().current().find(key);
    if (!p)
      return nullptr;
    return p->template try_cast<meta::Attribute<T>>();
  }

  meta::Attribute<float> &add_float(const std::string &key,
                                    const std::string &label,
                                    float              default_val,
                                    float              vmin,
                                    float              vmax,
                                    const std::string &value_format = "{:.2f}",
                                    bool               log_scale = false);

  meta::Attribute<int> &add_int(const std::string &key,
                                const std::string &label,
                                int                default_val,
                                int                vmin,
                                int                vmax,
                                const std::string &value_format = "{}");

  meta::Attribute<int> &add_seed(const std::string &key,
                                 const std::string &label = "Seed",
                                 unsigned int       default_val = 0);

  meta::Attribute<bool> &add_bool(const std::string &key,
                                  const std::string &label,
                                  bool               default_val = false);

  meta::Attribute<glm::vec2> &add_range(const std::string &key,
                                        const std::string &label,
                                        const glm::vec2   &default_range,
                                        float              vmin,
                                        float              vmax,
                                        bool               is_active = true,
                                        const std::string &value_format = "{:.3f}");

  meta::Attribute<int> &add_enum(const std::string                              &key,
                                 const std::string                              &label,
                                 const std::vector<std::pair<int, std::string>> &items,
                                 int default_val);

  meta::Attribute<int> &add_enum(const std::string                &key,
                                 const std::string                &label,
                                 const std::map<std::string, int> &enum_map,
                                 const std::string                &default_choice = "");

  meta::Attribute<glm::vec4> &add_color(const std::string &key,
                                        const std::string &label,
                                        const glm::vec4   &default_color);

  meta::Attribute<glm::vec2> &add_wavenumber(
      const std::string &key,
      const std::string &label = "Spatial Frequency",
      const glm::vec2   &default_val = {2.f, 2.f},
      float              vmin = 0.f,
      float              vmax = 64.f,
      bool               link_xy = true,
      const std::string &value_format = "{:.2f}");

  meta::Attribute<glm::vec2> &add_xy(const std::string &key,
                                     const std::string &label,
                                     const glm::vec2   &default_val,
                                     float              xmin,
                                     float              xmax,
                                     float              ymin,
                                     float              ymax);

  meta::ContainerGroup       &get_meta_group(); // lazily creates group + "main" container
  const meta::ContainerGroup &get_meta_group() const;
  void                        set_current_category(const std::string &category);
  const std::string &get_current_category() const { return this->current_category; }

  void                  finalize_attributes();
  const nlohmann::json &iinitial_meta_state() const { return this->initial_meta_state; }

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