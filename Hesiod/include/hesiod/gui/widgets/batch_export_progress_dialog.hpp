/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <string>
#include <vector>

#include "hesiod/gui/widgets/message_dialog.hpp"

class QLabel;
class QPushButton;
class QScrollArea;

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

class ProgressTrack;
class NodeStatusList;

// Bake progress, in the application's own dialog chrome: which variant is
// being baked, two progress tracks (variants overall, nodes of the current
// variant), what is computing right now, and every scheduled node with its
// state. Stays open at the end so the result can be read; Done closes it.
class BatchExportProgressDialog : public MessageDialog
{
  Q_OBJECT

public:
  explicit BatchExportProgressDialog(QWidget *parent = nullptr);

  bool is_canceled() const;
  void on_export_canceled();
  void on_export_finished();
  void on_export_failed(const std::string &error_msg);
  void on_node_finished(const std::string &node_id, bool success = true);
  void on_node_started(const std::string &node_id);
  void set_node_list(const std::vector<NodeExportStatus> &nodes);
  void set_output_folder(const QString &folder); // "Open folder" at the end
  void set_overall_progress(int current, int total);
  void set_variant(int                current_variant,
                   int                total_variants,
                   const std::string &variant_label);

signals:
  void request_cancel();

protected:
  void keyPressEvent(QKeyEvent *event) override;

private slots:
  void on_cancel_clicked();

private:
  void setup_layout();
  void set_status(const QString &text, int tone); // tone: 0 neutral, 1 done, -1 failed
  void finish();
  void fit_list_height();

  QLabel         *label_variant = nullptr;
  QLabel         *label_overall = nullptr;
  QLabel         *label_nodes = nullptr;
  QLabel         *label_status = nullptr;
  ProgressTrack  *progress_overall = nullptr;
  ProgressTrack  *progress_variant = nullptr;
  NodeStatusList *list = nullptr;
  QScrollArea    *list_scroll = nullptr;
  QPushButton    *cancel_button = nullptr;
  QPushButton    *done_button = nullptr;
  QPushButton    *open_button = nullptr;
  QString         output_folder;

  std::vector<NodeExportStatus> nodes;
  int                           current_variant = 0;
  int                           total_variants = 0;
  bool                          canceled = false;
  bool                          finished = false;
};

} // namespace hesiod
