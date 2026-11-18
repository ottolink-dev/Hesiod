/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <map>
#include <memory>
#include <string>
#include <unordered_map>

#include <QAction>
#include <QComboBox>
#include <QList>
#include <QPushButton>
#include <QSplitter>
#include <QWidget>

#include "nlohmann/json.hpp"

#include "gnodegui/graph_viewer.hpp"
#include "gnodegui/node_proxy.hpp"

#include "hesiod/gui/widgets/graph/coord_frame_widget.hpp"

namespace hesiod
{

class GraphNode;
class GraphManager;

// =====================================
// GraphNodeProxy
// =====================================
class GraphNodeProxy : public gngui::NodeProxy
{
public:
  GraphNodeProxy(std::weak_ptr<GraphNode> model, QObject *owner);

  std::string                                  get_id() const override;
  void                                         set_id(const std::string &id) override;
  std::string                                  get_caption() const override;
  std::string                                  get_category() const override;
  std::string                                  get_comment() const override;
  std::string                                  get_tool_tip_text() const override;
  int                                          get_nports() const override;
  std::string                                  get_port_caption(int index) const override;
  gngui::PortType                              get_port_type(int index) const override;
  std::string                                  get_data_type(int index) const override;
  void                                        *get_data_ref(int index) const override;
  std::unordered_map<std::string, std::string> consume_port_renames() override;

private:
  std::weak_ptr<GraphNode> model;
};

// =====================================
// GraphPreviewLayerWidget
// =====================================
class GraphPreviewLayerWidget : public QWidget
{
  Q_OBJECT

public:
  GraphPreviewLayerWidget(std::weak_ptr<GraphNode> p_graph_node,
                          QWidget                 *parent = nullptr);

  std::string get_current_bg_tag() const;
  GraphNode  *get_p_graph_node();

  void           json_from(nlohmann::json const &json);
  nlohmann::json json_to() const;

signals:
  void bg_image_updated(const std::string &graph_id, const QImage &image);
  void has_changed();

public slots:
  void on_combobox_changed();
  void update_combobox();

private:
  std::weak_ptr<GraphNode> p_graph_node;
  QComboBox               *combobox;
  std::string              current_bg_tag = "NONE";
};

// =====================================
// GraphManagerWidget
// =====================================
class GraphManagerWidget : public QWidget
{
  Q_OBJECT

public:
  GraphManagerWidget(std::weak_ptr<GraphManager> p_graph_manager,
                     QWidget                    *parent = nullptr);

  void clear();

  // --- Serialization ---
  void           json_from(nlohmann::json const &json);
  nlohmann::json json_to() const;

  // --- State Management ---
  void restore_window_state();
  void save_window_state() const;

signals:
  // --- UI ---
  void graph_removed();
  void has_changed();
  void list_reordered();
  void new_graph_added();
  void selected_graph_changed(const std::string &graph_id);
  void window_closed();

  // --- Graph ---
  void update_started();
  void update_finished();

public slots:
  // --- User Actions ---
  void on_apply_changes();
  void on_new_graph_request();
  void delete_graph(const std::string &graph_id);
  void on_reseed(bool backward = false);

  // --- Graph Actions ---
  void update_combobox(const std::string &graph_id);

private slots:
  void closeEvent(QCloseEvent *event) override;
  void hideEvent(QHideEvent *event) override;
  void showEvent(QShowEvent *event) override;

  void reset();
  void set_is_dirty(bool new_is_dirty);

private:
  void add_graph_item(const std::string &id, QPointF scene_pos = QPointF(0.f, 0.f));
  void setup_reset_actions();

  // --- Members ---
  std::weak_ptr<GraphManager>                      p_graph_manager;
  gngui::GraphViewer                              *graph_viewer = nullptr;
  CoordFrameWidget                                *coord_frame_widget = nullptr;
  QPushButton                                     *apply_button = nullptr;
  std::map<std::string, GraphPreviewLayerWidget *> preview_widgets;
  bool                                             is_dirty = false;
  QList<QAction *>                                 reset_actions;
};

} // namespace hesiod
