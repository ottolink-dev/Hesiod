/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <string>
#include <vector>

#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QProgressBar>
#include <QTableWidget>
#include <QVBoxLayout>

namespace hesiod
{

enum class NodeComputeState
{
  Pending,
  Computing,
  Completed,
  Failed
};

struct NodeExportStatus
{
  std::string      node_id;
  std::string      node_label;
  std::string      node_type;
  NodeComputeState state = NodeComputeState::Pending;
};

class BatchExportProgressDialog : public QDialog
{
  Q_OBJECT

public:
  explicit BatchExportProgressDialog(QWidget *parent = nullptr);

  void on_export_finished();
  void on_node_finished(const std::string &node_id, bool success = true);
  void on_node_started(const std::string &node_id);
  void set_node_list(const std::vector<NodeExportStatus> &nodes);
  void set_overall_progress(int current, int total);
  void set_variant(int                current_variant,
                   int                total_variants,
                   const std::string &variant_label);

private:
  void setup_layout();
  void update_node_row(int row);

  // ui elements
  QDialogButtonBox *button_box = nullptr;
  QLabel           *label_variant = nullptr;
  QLabel           *label_current_node = nullptr;
  QProgressBar     *progress_variant = nullptr;
  QProgressBar     *progress_overall = nullptr;
  QTableWidget     *table_nodes = nullptr;

  // data
  std::vector<NodeExportStatus> nodes;
  int                           current_variant = 0;
  int                           total_variants = 0;
};

} // namespace hesiod
