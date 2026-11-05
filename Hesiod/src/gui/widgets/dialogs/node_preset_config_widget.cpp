/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <filesystem>
#include <fstream>

#include <QDesktopServices>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QShowEvent>
#include <QSplitter>
#include <QUrl>

#include "meta_qt/ui/theme.hpp"

#include "hesiod/gui/theme/style_sheet.hpp"
#include "hesiod/gui/widgets/controls/control_tones.hpp"
#include "hesiod/gui/widgets/dialogs/node_preset_config_widget.hpp"
#include "hesiod/gui/widgets/utils/gui_utils.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/constants/node_preset_manager.hpp"

namespace fs = std::filesystem;

namespace hesiod
{

NodePresetConfigWidget::NodePresetConfigWidget(QWidget *parent) : QWidget(parent)
{
  Logger::log()->trace("NodePresetConfigWidget::NodePresetConfigWidget");

  auto *main_layout = new QVBoxLayout(this);
  main_layout->setContentsMargins(0, 0, 0, 0);

  auto *splitter = new QSplitter(Qt::Horizontal, this);
  splitter->setObjectName("presetSplitter");

  // --- Left pane: collection list & actions
  auto *left_card = new QFrame(splitter);
  left_card->setObjectName("presetCard");

  auto *left_layout = new QVBoxLayout(left_card);
  left_layout->setContentsMargins(10, 10, 10, 10);
  left_layout->setSpacing(8);

  auto *left_heading = new QLabel(tr("COLLECTIONS"), left_card);
  left_heading->setObjectName("cardHeading");
  left_layout->addWidget(left_heading);

  this->file_list = new QListWidget(left_card);
  this->file_list->setObjectName("presetFileList");
  this->file_list->setSelectionMode(QAbstractItemView::SingleSelection);
  left_layout->addWidget(this->file_list);

  auto *btn_layout = new QHBoxLayout();
  btn_layout->setSpacing(4);

  this->add_button = new QPushButton(tr("Add..."), left_card);
  this->add_button->setObjectName("secondaryButton");

  this->remove_button = new QPushButton(tr("Remove"), left_card);
  this->remove_button->setObjectName("secondaryButton");

  this->reload_button = new QPushButton(tr("Reload"), left_card);
  this->reload_button->setObjectName("secondaryButton");

  this->open_folder_button = new QPushButton(tr("Folder"), left_card);
  this->open_folder_button->setObjectName("secondaryButton");

  btn_layout->addWidget(this->add_button);
  btn_layout->addWidget(this->remove_button);
  btn_layout->addWidget(this->reload_button);
  btn_layout->addWidget(this->open_folder_button);

  left_layout->addLayout(btn_layout);

  // --- Right pane: details, tree & raw preview
  auto *right_card = new QFrame(splitter);
  right_card->setObjectName("presetCard");

  auto *right_layout = new QVBoxLayout(right_card);
  right_layout->setContentsMargins(10, 10, 10, 10);
  right_layout->setSpacing(8);

  auto *right_heading = new QLabel(tr("DETAILS & PRESETS"), right_card);
  right_heading->setObjectName("cardHeading");
  right_layout->addWidget(right_heading);

  this->file_title_label = new QLabel(tr("<b>No selection</b>"), right_card);
  this->file_title_label->setObjectName("fileTitle");

  this->file_author_label = new QLabel(right_card);
  this->file_author_label->setObjectName("fileMeta");

  this->file_comment_label = new QLabel(right_card);
  this->file_comment_label->setObjectName("fileMeta");
  this->file_comment_label->setWordWrap(true);

  this->file_path_label = new QLabel(right_card);
  this->file_path_label->setObjectName("filePath");
  this->file_path_label->setWordWrap(true);

  right_layout->addWidget(this->file_title_label);
  right_layout->addWidget(this->file_author_label);
  right_layout->addWidget(this->file_comment_label);
  right_layout->addWidget(this->file_path_label);

  this->preset_tree = new QTreeWidget(right_card);
  this->preset_tree->setObjectName("presetTree");
  this->preset_tree->setHeaderLabels({tr("Node / Preset"), tr("Count")});
  this->preset_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  this->preset_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  right_layout->addWidget(this->preset_tree);

  this->raw_json_view = new QTextEdit(right_card);
  this->raw_json_view->setObjectName("presetRawJson");
  this->raw_json_view->setReadOnly(true);
  this->raw_json_view->setVisible(false);
  this->raw_json_view->setFont(meta::qt::ui_font(11));
  right_layout->addWidget(this->raw_json_view);

  this->toggle_raw_button = new QPushButton(tr("View Raw JSON"), right_card);
  this->toggle_raw_button->setObjectName("secondaryButton");
  right_layout->addWidget(this->toggle_raw_button);

  splitter->addWidget(left_card);
  splitter->addWidget(right_card);
  splitter->setStretchFactor(0, 1);
  splitter->setStretchFactor(1, 2);

  main_layout->addWidget(splitter);

  this->apply_styling();

  // --- Connections
  this->connect(this->file_list,
                &QListWidget::currentRowChanged,
                this,
                &NodePresetConfigWidget::on_file_selection_changed);

  this->connect(this->file_list,
                &QListWidget::itemChanged,
                this,
                &NodePresetConfigWidget::on_item_changed);

  this->connect(this->add_button,
                &QPushButton::clicked,
                this,
                &NodePresetConfigWidget::on_add_file_clicked);

  this->connect(this->remove_button,
                &QPushButton::clicked,
                this,
                &NodePresetConfigWidget::on_remove_file_clicked);

  this->connect(this->reload_button,
                &QPushButton::clicked,
                this,
                &NodePresetConfigWidget::on_reload_clicked);

  this->connect(this->open_folder_button,
                &QPushButton::clicked,
                this,
                &NodePresetConfigWidget::on_open_folder_clicked);

  this->connect(this->toggle_raw_button,
                &QPushButton::clicked,
                this,
                &NodePresetConfigWidget::on_toggle_raw_json);

  this->refresh();
}

void NodePresetConfigWidget::on_add_file_clicked()
{
  QString file_path = QFileDialog::getOpenFileName(
      this,
      tr("Import Node Preset File"),
      QString(),
      tr("Preset Files (node_presets_*.json *.json)"));

  if (!file_path.isEmpty())
  {
    if (NodePresetManager::get_instance().import_preset_file(file_path.toStdString()))
      this->refresh();
  }
}

void NodePresetConfigWidget::on_file_selection_changed()
{
  this->update_details_panel(this->file_list->currentRow());
}

void NodePresetConfigWidget::on_item_changed(QListWidgetItem *item)
{
  if (!item)
    return;

  const int   row = this->file_list->row(item);
  const auto &files = NodePresetManager::get_instance().get_preset_files();
  if (row >= 0 && row < static_cast<int>(files.size()))
  {
    const bool is_checked = (item->checkState() == Qt::Checked);
    NodePresetManager::get_instance().set_file_enabled(files[row].file_path, is_checked);
  }
}

void NodePresetConfigWidget::on_open_folder_clicked()
{
  const std::string user_preset_path = NodePresetManager::get_instance()
                                           .get_user_preset_file_path();
  fs::path folder = fs::path(user_preset_path).parent_path();
  QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(folder.string())));
}

void NodePresetConfigWidget::on_reload_clicked()
{
  NodePresetManager::get_instance().rescan_files();
  this->refresh();
}

void NodePresetConfigWidget::on_remove_file_clicked()
{
  const int   row = this->file_list->currentRow();
  const auto &files = NodePresetManager::get_instance().get_preset_files();
  if (row < 0 || row >= static_cast<int>(files.size()))
    return;

  const auto &file_info = files[row];
  if (file_info.is_bundled)
  {
    QMessageBox::information(this,
                             tr("Cannot Remove"),
                             tr("Bundled preset files cannot be removed."));
    return;
  }

  const auto reply = QMessageBox::question(
      this,
      tr("Remove Preset File"),
      tr("Are you sure you want to delete '%1'?")
          .arg(QString::fromStdString(file_info.title)));

  if (reply == QMessageBox::Yes)
  {
    std::error_code ec;
    fs::remove(file_info.file_path, ec);
    NodePresetManager::get_instance().rescan_files();
    this->refresh();
  }
}

void NodePresetConfigWidget::on_toggle_raw_json()
{
  const bool show_raw = !this->raw_json_view->isVisible();
  this->raw_json_view->setVisible(show_raw);
  this->preset_tree->setVisible(!show_raw);
  this->toggle_raw_button->setText(show_raw ? tr("View Tree") : tr("View Raw JSON"));
}

void NodePresetConfigWidget::refresh()
{
  Logger::log()->trace("NodePresetConfigWidget::refresh");

  const int prev_row = this->file_list->currentRow();

  this->file_list->blockSignals(true);
  this->file_list->clear();

  const auto &files = NodePresetManager::get_instance().get_preset_files();

  for (const auto &file_info : files)
  {
    const std::string filename = fs::path(file_info.file_path).filename().string();
    QString           label = QString::fromStdString(filename);

    if (file_info.is_bundled)
      label += tr(" [bundled]");

    auto *item = new QListWidgetItem(label, this->file_list);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(file_info.enabled ? Qt::Checked : Qt::Unchecked);
  }

  this->file_list->blockSignals(false);

  int new_row = std::clamp(prev_row, 0, static_cast<int>(files.size()) - 1);
  if (!files.empty())
  {
    this->file_list->setCurrentRow(new_row);
    this->update_details_panel(new_row);
  }
  else
    this->update_details_panel(-1);
}

void NodePresetConfigWidget::update_details_panel(int index)
{
  const auto &files = NodePresetManager::get_instance().get_preset_files();
  if (index < 0 || index >= static_cast<int>(files.size()))
  {
    this->file_title_label->setText(tr("<b>No selection</b>"));
    this->file_author_label->clear();
    this->file_comment_label->clear();
    this->file_path_label->clear();
    this->preset_tree->clear();
    this->raw_json_view->clear();
    this->remove_button->setEnabled(false);
    return;
  }

  const auto &info = files[index];

  this->file_title_label->setText(
      QString("<b>%1</b>").arg(QString::fromStdString(info.title)));
  this->file_author_label->setText(
      info.author.empty() ? QString()
                          : tr("Author: %1").arg(QString::fromStdString(info.author)));
  this->file_comment_label->setText(
      info.comment.empty() ? QString()
                           : tr("Comment: %1").arg(QString::fromStdString(info.comment)));
  this->file_path_label->setText(QString::fromStdString(info.file_path));
  this->remove_button->setEnabled(!info.is_bundled);

  this->preset_tree->clear();
  for (const auto &[node_type, presets] : info.node_type_to_presets)
  {
    auto *node_item = new QTreeWidgetItem(this->preset_tree);
    node_item->setText(0, QString::fromStdString(node_type));
    node_item->setText(1, QString::number(presets.size()));

    for (const auto &preset_name : presets)
    {
      auto *preset_item = new QTreeWidgetItem(node_item);
      preset_item->setText(0, QString::fromStdString(preset_name));
    }
  }
  this->preset_tree->expandAll();

  // Load raw JSON for preview
  std::ifstream in(info.file_path);
  if (in.is_open())
  {
    std::string content((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
    this->raw_json_view->setPlainText(QString::fromStdString(content));
  }
  else
    this->raw_json_view->clear();
}

void NodePresetConfigWidget::showEvent(QShowEvent *event)
{
  QWidget::showEvent(event);
  this->apply_styling();
}

void NodePresetConfigWidget::apply_styling()
{
  const ControlTones t = control_tones();

  static constexpr std::string_view css_template =
#include "hesiod/gui/styles/node_preset_config.css"
      ;

  const std::map<std::string, QColor> color_tokens = {
      {"NAV_ACTIVE", mix_colors(t.sidebar, t.card, 0.85)},
      {"NAV_HOVER", mix_colors(t.sidebar, t.card, 0.45)},
      {"CARD", t.card},
      {"BORDER", t.border},
      {"FIELD", t.field},
      {"GHOST", mix_colors(t.sidebar, t.ink, 0.28)},
      {"FAINT", t.ink_faint},
      {"DIM", t.ink_dim},
      {"INK", t.ink}};

  QString css = render_stylesheet(css_template, {}, color_tokens);
  this->setStyleSheet(css);
}

} // namespace hesiod
