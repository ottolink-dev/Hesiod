/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <memory>
#include <vector>

#include <QPointer>

#include "meta/core/event.hpp"

#include "hesiod/gui/widgets/message_dialog.hpp"
#include "hesiod/model/graph/graph_config.hpp"

class QLabel;

namespace meta
{
class AttributeContainer;
}

namespace hesiod
{

// =====================================
// GraphConfigDialog
// =====================================

// A graph's resolution and computation settings, in the application's own
// dialog chrome with the properties panel's rows (the same section cards,
// switches and choices as the node settings). Edits `config` only when the
// dialog is accepted.
class GraphConfigDialog : public MessageDialog
{
public:
  GraphConfigDialog(GraphConfig &config, QWidget *parent = nullptr);
  ~GraphConfigDialog() override;

  void accept() override;

private:
  glm::ivec2 shape() const; // from the aspect / resolution / height choices
  void       update_summary();

  GraphConfig &config;

  std::unique_ptr<meta::AttributeContainer> domain;
  std::unique_ptr<meta::AttributeContainer> compute;
  std::vector<QPointer<QWidget>>            rows; // subscribe to the containers
  std::vector<meta::EventConnection>        connections;
  QLabel                                   *summary = nullptr;
};

} // namespace hesiod
