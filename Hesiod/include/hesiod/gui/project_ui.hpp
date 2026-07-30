/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QPointer>
#include <QWidget>

#include "nlohmann/json.hpp"

#include "qtd/texture_downloader.hpp"

#include "hesiod/gui/widgets/heightmapper_widget.hpp"

namespace hesiod
{

// forward
class Project;
class GraphManagerWidget;
class GraphTabsWidget;

// =====================================
// ProjectUI
// =====================================
class ProjectUI : public QWidget
{
  Q_OBJECT

public:
  explicit ProjectUI(QWidget *parent = nullptr);

  /// Deletes the parentless top-level windows - see the definition.
  ~ProjectUI() override;

  void load_ui_state(const std::string &fname);
  void save_ui_state(const std::string &fname) const;

  void cleanup();
  void initialize(ProjectModel *project);
  void setup_connections();

  QWidget                *get_widget();
  GraphManagerWidget     *get_graph_manager_widget_ref();
  GraphTabsWidget        *get_graph_tabs_widget_ref();
  HeightmapperWidget     *get_heightmapper_widget_ref();
  qtd::TextureDownloader *get_texture_downloader_ref();

private:
  GraphManagerWidget              *graph_manager_widget;
  GraphTabsWidget                 *graph_tabs_widget;
  QPointer<qtd::TextureDownloader> texture_downloader;
  QPointer<HeightmapperWidget>     heightmapper_widget;
};

} // namespace hesiod
