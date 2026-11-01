/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTextEdit>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace hesiod
{

class NodePresetConfigWidget : public QWidget
{
  Q_OBJECT

public:
  explicit NodePresetConfigWidget(QWidget *parent = nullptr);

  void refresh();

private slots:
  void on_add_file_clicked();
  void on_file_selection_changed();
  void on_item_changed(QListWidgetItem *item);
  void on_open_folder_clicked();
  void on_reload_clicked();
  void on_remove_file_clicked();
  void on_toggle_raw_json();

private:
  void update_details_panel(int index);

  QPushButton *add_button = nullptr;
  QPushButton *open_folder_button = nullptr;
  QPushButton *reload_button = nullptr;
  QPushButton *remove_button = nullptr;
  QPushButton *toggle_raw_button = nullptr;

  QListWidget *file_list = nullptr;
  QLabel      *file_title_label = nullptr;
  QLabel      *file_path_label = nullptr;
  QLabel      *file_author_label = nullptr;
  QLabel      *file_comment_label = nullptr;

  QTreeWidget *preset_tree = nullptr;
  QTextEdit   *raw_json_view = nullptr;
};

} // namespace hesiod
