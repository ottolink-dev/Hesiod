/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once
#include "gnodegui/node_proxy.hpp"

namespace hesiod
{
class BaseNode;

// The view owns the proxy; the graph owns the model.
class HesiodNodeProxy : public gngui::NodeProxy
{
public:
  HesiodNodeProxy(std::weak_ptr<BaseNode> model, QObject *owner);

  std::string     get_id() const override;
  void            set_id(const std::string &id) override;
  std::string     get_caption() const override;
  std::string     get_category() const override;
  std::string     get_comment() const override;
  std::string     get_tool_tip_text() const override;
  int             get_nports() const override;
  std::string     get_port_caption(int index) const override;
  gngui::PortType get_port_type(int index) const override;
  std::string     get_data_type(int index) const override;
  void           *get_data_ref(int index) const override;

private:
  std::weak_ptr<BaseNode> model;
};
} // namespace hesiod
