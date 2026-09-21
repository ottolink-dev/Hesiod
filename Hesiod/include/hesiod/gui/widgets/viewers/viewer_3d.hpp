/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <filesystem>

#include <QComboBox>
#include <QPointer>
#include <QWidget>

#include "nlohmann/json.hpp"

#include "qtr/render_widget.hpp"

#include "hesiod/gui/widgets/viewers/viewer.hpp"

namespace hesiod
{

class GraphNodeWidget; // forward

// =====================================
// Viewer3D
// =====================================
class Viewer3D : public Viewer
{
  Q_OBJECT

public:
  Viewer3D() = delete;
  Viewer3D(QPointer<GraphNodeWidget> p_graph_node_widget_, QWidget *parent = nullptr);

  // --- Serialization ---
  void           json_from(nlohmann::json const &json) override;
  nlohmann::json json_to() const override;

  void clear() override;
  bool get_param_visibility_state(const std::string &param_name) const override;
  void set_skybox(const std::filesystem::path path);
  void setup_layout() override;
  void setup_connections() override;

public slots:
  void on_view_param_visibility_changed(const std::string &param_name, bool new_state);

protected:
  void resizeEvent(QResizeEvent *) override;

private:
  ViewerNodeParam get_default_view_param() const override;
  void            update_renderer() override;

  ViewerType         viewer_type;
  qtr::RenderWidget *p_renderer = nullptr;
};

} // namespace hesiod