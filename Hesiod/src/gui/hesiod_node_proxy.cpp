/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "hesiod/gui/hesiod_node_proxy.hpp"

#include <utility>

#include "hesiod/model/nodes/base_node.hpp"

namespace hesiod
{
HesiodNodeProxy::HesiodNodeProxy(std::weak_ptr<BaseNode> model, QObject *owner)
    : model(std::move(model))
{
  this->setParent(owner);
}

std::string HesiodNodeProxy::get_id() const
{
  if (auto node = model.lock())
    return node->get_id();
  return {};
}

void HesiodNodeProxy::set_id(const std::string &id)
{
  if (auto node = model.lock())
    node->set_id(id);
}

std::string HesiodNodeProxy::get_caption() const
{
  if (auto node = model.lock())
    return node->get_label();
  return {};
}

std::string HesiodNodeProxy::get_category() const
{
  if (auto node = model.lock())
    return node->get_category();
  return {};
}

std::string HesiodNodeProxy::get_comment() const
{
  if (auto node = model.lock())
    return node->get_comment();
  return {};
}

std::string HesiodNodeProxy::get_tool_tip_text() const
{
  if (auto node = model.lock())
    return node->get_documentation_short_html();
  return {};
}

int HesiodNodeProxy::get_nports() const
{
  if (auto node = model.lock())
    return node->get_nports();
  return 0;
}

std::string HesiodNodeProxy::get_port_caption(int index) const
{
  if (auto node = model.lock())
    return node->get_port_label(index);
  return {};
}

gngui::PortType HesiodNodeProxy::get_port_type(int index) const
{
  if (auto node = model.lock())
    return node->get_port_type(index) == gnode::PortType::OUT ? gngui::PortType::OUT
                                                              : gngui::PortType::IN;
  return gngui::PortType::IN;
}

std::string HesiodNodeProxy::get_data_type(int index) const
{
  if (auto node = model.lock())
    return node->get_data_type(index);
  return {};
}

void *HesiodNodeProxy::get_data_ref(int index) const
{
  if (auto node = model.lock())
    return node->get_value_ref_void(index);
  return nullptr;
}
} // namespace hesiod
