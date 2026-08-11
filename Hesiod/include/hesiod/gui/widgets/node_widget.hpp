/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <memory>

#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include "hesiod/model/nodes/base_node.hpp"

namespace hesiod
{

class DataPreview; // forward decl.
class GraphNodeWidget;

// =====================================
// NodeWidget
// =====================================

// generic widget embedded within the GraphicsNode
class NodeWidget : public QWidget
{
  Q_OBJECT
public:
  NodeWidget(std::weak_ptr<BaseNode>   model,
             QPointer<GraphNodeWidget> p_gnw,
             const std::string        &msg = "",
             QWidget                  *parent = nullptr);

  QSize sizeHint() const override;

public slots:
  virtual void on_compute_finished();
  virtual void on_compute_started();

protected:
  void setup_connections();
  void setup_layout();

  std::weak_ptr<BaseNode>   model;
  QPointer<GraphNodeWidget> p_gnw;
  std::string               msg;

  QVBoxLayout *layout;
  DataPreview *data_preview;

  gnode::EventConnection before_conn;
  gnode::EventConnection after_conn;
};

// DebugNodeWidget
class DebugNodeWidget : public NodeWidget
{
public:
  DebugNodeWidget(std::weak_ptr<BaseNode>   model,
                  QPointer<GraphNodeWidget> p_gnw,
                  const std::string        &msg = "",
                  QWidget                  *parent = nullptr);

  void on_compute_finished() override;

private:
  QLabel *label;
};

// ExportNodeWidget
class ExportNodeWidget : public NodeWidget
{
public:
  ExportNodeWidget(std::weak_ptr<BaseNode>   model,
                   QPointer<GraphNodeWidget> p_gnw,
                   const std::string        &msg = "",
                   QWidget                  *parent = nullptr);
};

// ThruNodeWidget
class ThruNodeWidget : public NodeWidget
{
public:
  ThruNodeWidget(std::weak_ptr<BaseNode>   model,
                 QPointer<GraphNodeWidget> p_gnw,
                 const std::string        &msg = "",
                 QWidget                  *parent = nullptr);

  void on_compute_finished() override;

private:
  QPushButton *button;
};

// ToggleNodeWidget
class ToggleNodeWidget : public NodeWidget
{
public:
  ToggleNodeWidget(std::weak_ptr<BaseNode>   model,
                   QPointer<GraphNodeWidget> p_gnw,
                   const std::string        &msg = "",
                   QWidget                  *parent = nullptr);

  void on_compute_finished() override;

private:
  QPushButton *button_a;
  QPushButton *button_b;
  bool         sync_guard = false;
};

// =====================================
// Factory
// =====================================

QWidget *node_widget_factory(const std::string        &node_type,
                             std::weak_ptr<BaseNode>   model,
                             QPointer<GraphNodeWidget> p_gnw,
                             QWidget                  *parent = nullptr);

} // namespace hesiod