/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <format>

#include <QCoreApplication>
#include <QHeaderView>
#include <QTableWidgetItem>

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/batch_export_progress_dialog.hpp"
#include "hesiod/gui/widgets/gui_utils.hpp"
#include "hesiod/logger.hpp"

namespace hesiod
{

BatchExportProgressDialog::BatchExportProgressDialog(QWidget *parent) : QDialog(parent)
{
  Logger::log()->trace("BatchExportProgressDialog::BatchExportProgressDialog");

  this->setWindowTitle("Hesiod - Batch Export Progress");
  this->setWindowModality(Qt::ApplicationModal);
  this->resize(520, 480);

  this->setup_layout();
}

void BatchExportProgressDialog::on_export_finished()
{
  if (this->label_current_node)
    this->label_current_node->setText("Export completed.");

  if (this->button_box)
    this->button_box->setEnabled(true);

  QCoreApplication::processEvents();
}

void BatchExportProgressDialog::on_node_finished(const std::string &node_id, bool success)
{
  for (size_t i = 0; i < this->nodes.size(); ++i)
  {
    if (this->nodes[i].node_id == node_id)
    {
      this->nodes[i].state = success ? NodeComputeState::Completed
                                     : NodeComputeState::Failed;
      this->update_node_row(static_cast<int>(i));
      break;
    }
  }

  // update variant progress bar
  int completed_count = 0;
  for (const auto &n : this->nodes)
  {
    if (n.state == NodeComputeState::Completed || n.state == NodeComputeState::Failed)
      completed_count++;
  }

  if (this->progress_variant)
    this->progress_variant->setValue(completed_count);

  QCoreApplication::processEvents();
}

void BatchExportProgressDialog::on_node_started(const std::string &node_id)
{
  for (size_t i = 0; i < this->nodes.size(); ++i)
  {
    if (this->nodes[i].node_id == node_id)
    {
      this->nodes[i].state = NodeComputeState::Computing;
      this->update_node_row(static_cast<int>(i));

      if (this->table_nodes)
        this->table_nodes->scrollToItem(this->table_nodes->item(static_cast<int>(i), 0));

      if (this->label_current_node)
      {
        std::string txt = std::format("Computing: {} ({})",
                                      this->nodes[i].node_label,
                                      this->nodes[i].node_id);
        this->label_current_node->setText(QString::fromStdString(txt));
      }
      break;
    }
  }

  QCoreApplication::processEvents();
}

void BatchExportProgressDialog::set_node_list(
    const std::vector<NodeExportStatus> &new_nodes)
{
  this->nodes = new_nodes;

  if (this->table_nodes)
  {
    this->table_nodes->setRowCount(static_cast<int>(this->nodes.size()));
    for (size_t i = 0; i < this->nodes.size(); ++i)
    {
      QTableWidgetItem *item_status = new QTableWidgetItem();
      QTableWidgetItem *item_label = new QTableWidgetItem(
          QString::fromStdString(this->nodes[i].node_label));
      QTableWidgetItem *item_id = new QTableWidgetItem(
          QString::fromStdString(this->nodes[i].node_id));

      item_status->setTextAlignment(Qt::AlignCenter);
      item_id->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

      this->table_nodes->setItem(static_cast<int>(i), 0, item_status);
      this->table_nodes->setItem(static_cast<int>(i), 1, item_label);
      this->table_nodes->setItem(static_cast<int>(i), 2, item_id);

      this->update_node_row(static_cast<int>(i));
    }
  }

  if (this->progress_variant)
  {
    this->progress_variant->setRange(0, static_cast<int>(this->nodes.size()));
    this->progress_variant->setValue(0);
  }

  if (this->label_current_node)
    this->label_current_node->setText("Starting variant computation...");

  QCoreApplication::processEvents();
}

void BatchExportProgressDialog::set_overall_progress(int current, int total)
{
  if (this->progress_overall)
  {
    this->progress_overall->setRange(0, total);
    this->progress_overall->setValue(current);
  }
  QCoreApplication::processEvents();
}

void BatchExportProgressDialog::set_variant(int                variant_idx,
                                            int                n_variants,
                                            const std::string &variant_name)
{
  this->current_variant = variant_idx;
  this->total_variants = n_variants;

  if (this->label_variant)
  {
    std::string text = std::format("Variant {}/{}: {}",
                                   variant_idx,
                                   n_variants,
                                   variant_name);
    this->label_variant->setText(QString::fromStdString(text));
  }

  this->set_overall_progress(variant_idx, n_variants);
  QCoreApplication::processEvents();
}

void BatchExportProgressDialog::setup_layout()
{
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setSpacing(8);

  // --- Header section: variant information & overall progress

  this->label_variant = new QLabel("Preparing export...", this);
  QFont font_header = this->label_variant->font();
  font_header.setBold(true);
  font_header.setPointSize(font_header.pointSize() + 1);
  this->label_variant->setFont(font_header);
  layout->addWidget(this->label_variant);

  this->progress_overall = new QProgressBar(this);
  this->progress_overall->setRange(0, 100);
  this->progress_overall->setValue(0);
  this->progress_overall->setTextVisible(true);
  this->progress_overall->setFormat("Overall: %v/%m variants (%p%)");
  layout->addWidget(this->progress_overall);

  // --- Current node info & variant progress

  this->label_current_node = new QLabel("Idle", this);
  std::string secondary_style = std::format(
      "color: {};",
      HSD_CTX.app_settings.colors.text_secondary.name().toStdString());
  this->label_current_node->setStyleSheet(secondary_style.c_str());
  layout->addWidget(this->label_current_node);

  this->progress_variant = new QProgressBar(this);
  this->progress_variant->setRange(0, 100);
  this->progress_variant->setValue(0);
  this->progress_variant->setTextVisible(true);
  this->progress_variant->setFormat("Variant nodes: %v/%m (%p%)");
  layout->addWidget(this->progress_variant);

  // --- Nodes table

  this->table_nodes = new QTableWidget(this);
  this->table_nodes->setColumnCount(3);
  this->table_nodes->setHorizontalHeaderLabels({"Status", "Node", "ID"});
  this->table_nodes->horizontalHeader()->setSectionResizeMode(
      0,
      QHeaderView::ResizeToContents);
  this->table_nodes->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
  this->table_nodes->horizontalHeader()->setSectionResizeMode(
      2,
      QHeaderView::ResizeToContents);
  this->table_nodes->verticalHeader()->setVisible(false);
  this->table_nodes->setSelectionMode(QAbstractItemView::NoSelection);
  this->table_nodes->setEditTriggers(QAbstractItemView::NoEditTriggers);
  this->table_nodes->setShowGrid(false);
  this->table_nodes->setAlternatingRowColors(true);

  layout->addWidget(this->table_nodes, 1);

  // --- Dialog buttons

  this->button_box = new QDialogButtonBox(QDialogButtonBox::Ok, this);
  this->button_box->setEnabled(false);
  this->connect(this->button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
  layout->addWidget(this->button_box);

  this->setLayout(layout);
}

void BatchExportProgressDialog::update_node_row(int row)
{
  if (!this->table_nodes || row < 0 || row >= static_cast<int>(this->nodes.size()))
    return;

  QTableWidgetItem *item_status = this->table_nodes->item(row, 0);
  QTableWidgetItem *item_label = this->table_nodes->item(row, 1);
  QTableWidgetItem *item_id = this->table_nodes->item(row, 2);

  if (!item_status || !item_label || !item_id)
    return;

  const auto &node_status = this->nodes[static_cast<size_t>(row)];

  switch (node_status.state)
  {
  case NodeComputeState::Pending:
    item_status->setText(QString::fromUtf8("⏳"));
    item_status->setForeground(HSD_CTX.app_settings.colors.text_secondary);
    item_label->setForeground(HSD_CTX.app_settings.colors.text_secondary);
    item_id->setForeground(HSD_CTX.app_settings.colors.text_secondary);
    break;

  case NodeComputeState::Computing:
    item_status->setText(QString::fromUtf8("⚙️"));
    item_status->setForeground(HSD_CTX.app_settings.colors.accent);
    item_label->setForeground(HSD_CTX.app_settings.colors.text_primary);
    item_id->setForeground(HSD_CTX.app_settings.colors.text_primary);
    break;

  case NodeComputeState::Completed:
    item_status->setText(QString::fromUtf8("✓"));
    item_status->setForeground(QColor("#73D216")); // green
    item_label->setForeground(HSD_CTX.app_settings.colors.text_primary);
    item_id->setForeground(HSD_CTX.app_settings.colors.text_secondary);
    break;

  case NodeComputeState::Failed:
    item_status->setText(QString::fromUtf8("✗"));
    item_status->setForeground(QColor("#CC0000")); // red
    item_label->setForeground(QColor("#CC0000"));
    item_id->setForeground(QColor("#CC0000"));
    break;
  }
}

} // namespace hesiod
