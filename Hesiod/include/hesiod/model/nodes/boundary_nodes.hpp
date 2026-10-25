/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <memory>
#include <string>

#include "gnode/subgraph.hpp"
#include "highmap/coord_frame.hpp"
#include "highmap/interpolate/interpolate_array.hpp"
#include "highmap/virtual_array/virtual_array.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/graph/graph_config.hpp"
#include "hesiod/model/nodes/base_node.hpp"

namespace hesiod
{

// =====================================
// NodeIn
// =====================================
template <typename T>
class NodeIn : public BaseNode, public gnode::ISubGraphInputNode
{
public:
  NodeIn(const std::string &slot_name, std::weak_ptr<GraphConfig> config)
      : BaseNode("NodeIn", config), slot_name(slot_name)
  {
    this->add_port<T>(gnode::PortType::OUT, "value", CONFIG((*this)));
  }

  void compute() override {}

  std::string get_slot_name() const override { return this->slot_name; }

  void set_slot_name(const std::string &new_name) { this->slot_name = new_name; }

  void pull_from_parent(gnode::Node *parent_node, const std::string &port_label) override
  {
    if (!parent_node)
      return;

    T *parent_val = parent_node->get_value_ref<T>(port_label);
    T *inner_val  = this->get_value_ref<T>("value");

    if (parent_val && inner_val)
    {
      if constexpr (std::is_same_v<T, hmap::VirtualArray>)
      {
        const hmap::CoordFrame *parent_frame = dynamic_cast<const hmap::CoordFrame *>(parent_node);
        const BaseNode         *parent_base  = dynamic_cast<const BaseNode *>(parent_node);

        const hmap::CoordFrame *inner_frame  = dynamic_cast<const hmap::CoordFrame *>(this->get_p_graph());

        bool frames_differ = false;
        if (parent_frame && inner_frame)
        {
          frames_differ = (parent_frame->get_origin() != inner_frame->get_origin()) ||
                          (parent_frame->get_size() != inner_frame->get_size()) ||
                          (parent_frame->get_rotation_angle() != inner_frame->get_rotation_angle());
        }

        bool configs_differ = false;
        if (parent_base)
        {
          configs_differ = (parent_base->cfg().shape != this->cfg().shape);
        }

        if (frames_differ || configs_differ)
        {
          // on-the-fly spatial interpolation
          hmap::CoordFrame default_frame;
          const hmap::CoordFrame &src_frame = parent_frame ? *parent_frame : default_frame;
          const hmap::CoordFrame &dst_frame = inner_frame ? *inner_frame : default_frame;

          hmap::interpolate_heightmap(*parent_val, *inner_val, src_frame, dst_frame, this->cfg().cm_cpu);
          this->is_dirty = true;
          return;
        }

        // fast path: identical frames & resolution
        inner_val->copy_from(*parent_val, this->cfg().cm_cpu, true);
        this->is_dirty = true;
      }
      else
      {
        if (*inner_val != *parent_val)
        {
          *inner_val = *parent_val;
          this->is_dirty = true;
        }
      }
    }
  }

  void json_from(nlohmann::json const &json) override
  {
    BaseNode::json_from(json);
    if (json.contains("slot_name"))
      this->slot_name = json["slot_name"];
  }

  nlohmann::json json_to() const override
  {
    nlohmann::json json = BaseNode::json_to();
    json["slot_name"] = this->slot_name;
    return json;
  }

private:
  std::string slot_name;
};

// =====================================
// NodeOut
// =====================================
template <typename T>
class NodeOut : public BaseNode, public gnode::ISubGraphOutputNode
{
public:
  NodeOut(const std::string &slot_name, std::weak_ptr<GraphConfig> config)
      : BaseNode("NodeOut", config), slot_name(slot_name)
  {
    this->add_port<T>(gnode::PortType::IN, "value", CONFIG((*this)));
  }

  void compute() override {}

  std::string get_slot_name() const override { return this->slot_name; }

  void set_slot_name(const std::string &new_name) { this->slot_name = new_name; }

  void push_to_parent(gnode::Node *parent_node, const std::string &port_label) override
  {
    if (!parent_node)
      return;

    T *inner_val  = this->get_value_ref<T>("value");
    T *parent_val = parent_node->get_value_ref<T>(port_label);

    if (inner_val && parent_val)
    {
      if constexpr (std::is_same_v<T, hmap::VirtualArray>)
      {
        parent_val->copy_from(*inner_val, this->cfg().cm_cpu, true);
      }
      else
      {
        *parent_val = *inner_val;
      }
    }
  }

  void json_from(nlohmann::json const &json) override
  {
    BaseNode::json_from(json);
    if (json.contains("slot_name"))
      this->slot_name = json["slot_name"];
  }

  nlohmann::json json_to() const override
  {
    nlohmann::json json = BaseNode::json_to();
    json["slot_name"] = this->slot_name;
    return json;
  }

private:
  std::string slot_name;
};

} // namespace hesiod
