/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QLabel>
#include <QPushButton>

#include "hesiod/gui/widgets/node_widgets/node_widget.hpp"
#include "hesiod/model/nodes/subgraph_node.hpp"

namespace hesiod
{

// =====================================
// SubGraphNodeWidget
// =====================================
class SubGraphNodeWidget : public NodeWidget
{
  Q_OBJECT

public:
  SubGraphNodeWidget(std::weak_ptr<BaseNode>   model,
                     QPointer<GraphNodeWidget> p_gnw,
                     const std::string        &msg = "",
                     QWidget                  *parent = nullptr);

  void on_compute_finished() override;
  void on_compute_started() override;
  void update_node_summary();

signals:
  void open_subgraph_requested(const std::string &node_id);

protected:
  void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
  void setup_subgraph_ui();

  QLabel      *summary_label = nullptr;
  QPushButton *open_button = nullptr;
};

} // namespace hesiod
