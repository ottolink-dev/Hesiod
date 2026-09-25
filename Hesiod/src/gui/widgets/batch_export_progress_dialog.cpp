/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <cmath>
#include <format>

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QVariantAnimation>

#include "meta_qt/ui/theme.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/batch_export_progress_dialog.hpp"
#include "hesiod/logger.hpp"

namespace hesiod
{

namespace
{

QColor mix(const QColor &a, const QColor &b, qreal t)
{
  t = std::clamp(t, 0.0, 1.0);
  return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
                          a.greenF() + (b.greenF() - a.greenF()) * t,
                          a.blueF() + (b.blueF() - a.blueF()) * t,
                          a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}

const QColor kDone("#7cc46a");
const QColor kFailed("#e0605a");

} // namespace

// =====================================
// ProgressTrack: a rounded track whose fill glides to each new value
// =====================================

class ProgressTrack final : public QWidget
{
public:
  explicit ProgressTrack(QWidget *parent) : QWidget(parent)
  {
    this->setFixedHeight(8);
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    this->glide = new QVariantAnimation(this);
    this->glide->setEasingCurve(QEasingCurve::OutCubic);
    QObject::connect(this->glide,
                     &QVariantAnimation::valueChanged,
                     this,
                     [this](const QVariant &v)
                     {
                       this->shown = v.toReal();
                       this->update();
                     });
  }

  void set_progress(int value, int maximum)
  {
    const qreal to = maximum > 0 ? std::clamp(qreal(value) / maximum, 0.0, 1.0) : 0.0;
    this->glide->stop();
    this->glide->setDuration(HSD_CTX.app_settings.interface.enable_ui_animations ? 320
                                                                                 : 1);
    this->glide->setStartValue(this->shown);
    this->glide->setEndValue(to);
    this->glide->start();
  }

  void set_tone(int new_tone)
  {
    this->tone = new_tone;
    this->update();
  }

protected:
  void paintEvent(QPaintEvent *) override
  {
    const auto &colors = HSD_CTX.app_settings.colors;
    QPainter    p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF track = QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal  r = track.height() / 2.0;

    p.setPen(Qt::NoPen);
    p.setBrush(mix(colors.bg_deep, colors.bg_primary, 0.35));
    p.drawRoundedRect(track, r, r);

    if (this->shown <= 0.0)
      return;

    const QColor base = this->tone > 0 ? kDone : this->tone < 0 ? kFailed : colors.accent;
    QRectF       fill = track;
    fill.setWidth(std::max(track.height(), track.width() * this->shown));

    QLinearGradient g(fill.topLeft(), fill.topRight());
    g.setColorAt(0.0, base.darker(115));
    g.setColorAt(1.0, base.lighter(115));
    p.setBrush(g);
    p.drawRoundedRect(fill, r, r);
  }

private:
  QVariantAnimation *glide = nullptr;
  qreal              shown = 0.0;
  int                tone = 0;
};

// =====================================
// NodeStatusList: one painted row per scheduled node
// =====================================

class NodeStatusList final : public QWidget
{
public:
  explicit NodeStatusList(std::vector<NodeExportStatus> &nodes, QWidget *parent)
      : QWidget(parent), nodes(nodes)
  {
    this->setAttribute(Qt::WA_NoSystemBackground);

    // the spinner of the computing row turns while the bake pumps events
    this->spin = new QTimer(this);
    this->spin->setInterval(16);
    QObject::connect(this->spin,
                     &QTimer::timeout,
                     this,
                     [this]()
                     {
                       this->angle = std::fmod(this->angle + 7.0, 360.0);
                       this->update();
                     });
  }

  static constexpr int kRow = 30;

  void refresh()
  {
    this->setMinimumHeight(int(this->nodes.size()) * kRow + 4);
    const bool computing = std::any_of(
        this->nodes.begin(),
        this->nodes.end(),
        [](const NodeExportStatus &n) { return n.state == NodeComputeState::Computing; });
    if (computing && !this->spin->isActive())
      this->spin->start();
    else if (!computing)
      this->spin->stop();
    this->update();
  }

  QRect row_rect(int row) const { return QRect(0, 2 + row * kRow, this->width(), kRow); }

protected:
  void paintEvent(QPaintEvent *event) override
  {
    const auto &colors = HSD_CTX.app_settings.colors;
    QPainter    p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QFont label_font = meta::qt::ui_font(12);
    const QFont id_font = meta::qt::mono_font(11);

    for (int i = 0; i < int(this->nodes.size()); ++i)
    {
      const QRect row = this->row_rect(i);
      if (!row.intersects(event->rect()))
        continue;

      const NodeExportStatus &n = this->nodes[i];
      const bool              active = n.state == NodeComputeState::Computing;
      const bool              pending = n.state == NodeComputeState::Pending;

      if (active)
      {
        p.setPen(Qt::NoPen);
        p.setBrush(mix(colors.bg_primary, colors.accent, 0.16));
        p.drawRoundedRect(QRectF(row).adjusted(2, 1, -2, -1), 7, 7);
      }

      // status mark
      const QPointF c(row.left() + 20, row.center().y() + 0.5);
      switch (n.state)
      {
      case NodeComputeState::Pending:
        p.setPen(QPen(mix(colors.bg_primary, colors.text_primary, 0.30), 1.4));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(c, 5.5, 5.5);
        break;
      case NodeComputeState::Computing:
      {
        p.setPen(QPen(mix(colors.bg_primary, colors.accent, 0.35), 2.0));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(c, 6.0, 6.0);
        QPen arc(colors.accent.lighter(130), 2.0);
        arc.setCapStyle(Qt::RoundCap);
        p.setPen(arc);
        p.drawArc(QRectF(c.x() - 6, c.y() - 6, 12, 12), int(-this->angle * 16), 100 * 16);
        break;
      }
      case NodeComputeState::Completed:
      {
        p.setPen(Qt::NoPen);
        p.setBrush(mix(colors.bg_primary, kDone, 0.85));
        p.drawEllipse(c, 7.0, 7.0);
        QPen tick(QColor(20, 30, 20), 1.7);
        tick.setCapStyle(Qt::RoundCap);
        tick.setJoinStyle(Qt::RoundJoin);
        p.setPen(tick);
        QPainterPath path;
        path.moveTo(c + QPointF(-3.2, 0.2));
        path.lineTo(c + QPointF(-0.9, 2.5));
        path.lineTo(c + QPointF(3.4, -2.3));
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
        break;
      }
      case NodeComputeState::Failed:
      {
        p.setPen(Qt::NoPen);
        p.setBrush(kFailed);
        p.drawEllipse(c, 7.0, 7.0);
        QPen cross(QColor(40, 16, 16), 1.7);
        cross.setCapStyle(Qt::RoundCap);
        p.setPen(cross);
        p.drawLine(c + QPointF(-2.6, -2.6), c + QPointF(2.6, 2.6));
        p.drawLine(c + QPointF(2.6, -2.6), c + QPointF(-2.6, 2.6));
        break;
      }
      }

      // label and id
      const QColor ink = n.state == NodeComputeState::Failed ? kFailed
                         : pending ? mix(colors.bg_primary, colors.text_primary, 0.55)
                                   : colors.text_primary;
      p.setFont(label_font);
      p.setPen(ink);
      const QRect text_rect = row.adjusted(40, 0, -60, 0);
      p.drawText(text_rect,
                 Qt::AlignVCenter | Qt::AlignLeft,
                 p.fontMetrics().elidedText(QString::fromStdString(n.node_label),
                                            Qt::ElideRight,
                                            text_rect.width()));

      p.setFont(id_font);
      p.setPen(mix(colors.bg_primary, colors.text_primary, 0.40));
      p.drawText(row.adjusted(0, 0, -12, 0),
                 Qt::AlignVCenter | Qt::AlignRight,
                 QString("#%1").arg(QString::fromStdString(n.node_id)));
    }
  }

private:
  std::vector<NodeExportStatus> &nodes;
  QTimer                        *spin = nullptr;
  qreal                          angle = 0.0;
};

// =====================================
// BatchExportProgressDialog
// =====================================

BatchExportProgressDialog::BatchExportProgressDialog(QWidget *parent)
    : MessageDialog(parent,
                    Kind::None,
                    "Baking and exporting",
                    "Each variant is computed at full resolution, then written by the "
                    "export nodes.")
{
  Logger::log()->trace("BatchExportProgressDialog::BatchExportProgressDialog");

  this->setWindowTitle("Hesiod - Bake and Export");
  this->setWindowModality(Qt::ApplicationModal);
  this->set_card_width(500);

  this->setup_layout();
}

bool BatchExportProgressDialog::is_canceled() const { return this->canceled; }

void BatchExportProgressDialog::keyPressEvent(QKeyEvent *event)
{
  // once the bake is over, Escape and Enter just close
  if (this->finished && (event->key() == Qt::Key_Escape ||
                         event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter))
  {
    this->accept();
    return;
  }
  MessageDialog::keyPressEvent(event);
}

void BatchExportProgressDialog::on_cancel_clicked()
{
  if (this->canceled || this->finished)
    return;

  this->canceled = true;
  this->cancel_button->setEnabled(false);
  this->cancel_button->setText("Canceling...");
  this->set_status("Canceling the bake...", 0);

  emit request_cancel();

  QCoreApplication::processEvents();
}

void BatchExportProgressDialog::set_status(const QString &text, int tone)
{
  const auto  &colors = HSD_CTX.app_settings.colors;
  const QColor ink = tone > 0   ? kDone
                     : tone < 0 ? kFailed
                                : mix(colors.bg_primary, colors.text_primary, 0.70);
  this->label_status->setText(text);
  this->label_status->setStyleSheet(
      QString("color: %1; font-size: 12px; background: transparent;%2")
          .arg(ink.name(), tone != 0 ? " font-weight: 600;" : ""));
}

void BatchExportProgressDialog::set_output_folder(const QString &folder)
{
  this->output_folder = folder;
}

void BatchExportProgressDialog::fit_list_height()
{
  // as tall as its rows, up to nine; beyond that it scrolls
  const int rows = std::clamp(int(this->nodes.size()), 3, 9);
  this->list_scroll->setFixedHeight(rows * NodeStatusList::kRow + 12);
}

void BatchExportProgressDialog::finish()
{
  this->finished = true;
  this->cancel_button->hide();
  this->done_button->show();
  this->done_button->setEnabled(true);
  if (!this->output_folder.isEmpty() && QDir(this->output_folder).exists())
  {
    this->open_button->setToolTip(QDir::toNativeSeparators(this->output_folder));
    this->open_button->show();
  }
  this->done_button->setFocus();
  this->list->refresh();
  QCoreApplication::processEvents();
}

void BatchExportProgressDialog::on_export_canceled()
{
  this->canceled = true;
  this->set_status("Bake canceled.", -1);
  this->progress_variant->set_tone(-1);
  this->finish();
}

void BatchExportProgressDialog::on_export_finished()
{
  this->set_status(
      this->total_variants > 1
          ? QString("Done: %1 variants baked and exported.").arg(this->total_variants)
          : QString("Done: baked and exported."),
      1);
  this->progress_overall->set_tone(1);
  this->progress_variant->set_tone(1);
  this->finish();
}

void BatchExportProgressDialog::on_export_failed(const std::string &error_msg)
{
  this->set_status(QString::fromStdString(error_msg), -1);
  this->progress_variant->set_tone(-1);
  this->finish();
}

void BatchExportProgressDialog::on_node_finished(const std::string &node_id, bool success)
{
  for (auto &n : this->nodes)
    if (n.node_id == node_id)
    {
      n.state = success ? NodeComputeState::Completed : NodeComputeState::Failed;
      break;
    }

  const int done = int(std::count_if(this->nodes.begin(),
                                     this->nodes.end(),
                                     [](const NodeExportStatus &n)
                                     {
                                       return n.state == NodeComputeState::Completed ||
                                              n.state == NodeComputeState::Failed;
                                     }));
  this->progress_variant->set_progress(done, int(this->nodes.size()));
  this->label_nodes->setText(QString("%1 / %2").arg(done).arg(this->nodes.size()));
  this->list->refresh();

  QCoreApplication::processEvents();
}

void BatchExportProgressDialog::on_node_started(const std::string &node_id)
{
  for (size_t i = 0; i < this->nodes.size(); ++i)
    if (this->nodes[i].node_id == node_id)
    {
      this->nodes[i].state = NodeComputeState::Computing;
      this->list->refresh();

      // keep the computing row in view
      const QRect row = this->list->row_rect(int(i));
      this->list_scroll->ensureVisible(row.center().x(),
                                       row.center().y(),
                                       0,
                                       row.height());

      this->set_status(
          QString("Computing %1").arg(QString::fromStdString(this->nodes[i].node_label)),
          0);
      break;
    }

  QCoreApplication::processEvents();
}

void BatchExportProgressDialog::set_node_list(
    const std::vector<NodeExportStatus> &new_nodes)
{
  this->nodes = new_nodes;
  this->list->refresh();
  this->fit_list_height();
  this->list_scroll->verticalScrollBar()->setValue(0);

  this->progress_variant->set_tone(0);
  this->progress_variant->set_progress(0, int(this->nodes.size()));
  this->label_nodes->setText(QString("0 / %1").arg(this->nodes.size()));
  this->set_status("Starting...", 0);

  QCoreApplication::processEvents();
}

void BatchExportProgressDialog::set_overall_progress(int current, int total)
{
  this->progress_overall->set_progress(current, total);
  QCoreApplication::processEvents();
}

void BatchExportProgressDialog::set_variant(int                variant_idx,
                                            int                n_variants,
                                            const std::string &variant_name)
{
  this->current_variant = variant_idx;
  this->total_variants = n_variants;

  this->label_variant->setText(QString::fromStdString(variant_name));
  this->label_overall->setText(
      QString("Variant %1 of %2").arg(variant_idx).arg(n_variants));

  // the overall track counts finished variants
  this->set_overall_progress(variant_idx - 1, n_variants);
}

void BatchExportProgressDialog::setup_layout()
{
  const auto   &colors = HSD_CTX.app_settings.colors;
  const QString dim = mix(colors.bg_primary, colors.text_primary, 0.60).name();

  QVBoxLayout *body = this->body();
  body->setSpacing(8);

  const auto caption_row = [&](QLabel *&left, QLabel *&right, const QString &left_text)
  {
    auto *row = new QHBoxLayout();
    left = new QLabel(left_text, this);
    right = new QLabel(this);
    left->setStyleSheet(
        QString("color: %1; font-size: 13px; font-weight: 600; background: transparent;")
            .arg(colors.text_primary.name()));
    right->setStyleSheet(
        QString("color: %1; font-size: 12px; background: transparent;").arg(dim));
    row->addWidget(left);
    row->addStretch(1);
    row->addWidget(right);
    body->addLayout(row);
  };

  // --- which variant, and the variants overall
  caption_row(this->label_variant, this->label_overall, "Preparing...");
  this->progress_overall = new ProgressTrack(this);
  body->addWidget(this->progress_overall);

  body->addSpacing(6);

  // --- the current variant's nodes
  QLabel *nodes_title = nullptr;
  caption_row(nodes_title, this->label_nodes, "Nodes");
  this->progress_variant = new ProgressTrack(this);
  body->addWidget(this->progress_variant);

  this->label_status = new QLabel(this);
  this->label_status->setWordWrap(true);
  body->addWidget(this->label_status);
  this->set_status("Starting...", 0);

  // --- every scheduled node
  this->list = new NodeStatusList(this->nodes, nullptr);
  this->list_scroll = new QScrollArea(this);
  this->list_scroll->setObjectName("bakeNodeList");
  this->list_scroll->setWidget(this->list);
  this->list_scroll->setWidgetResizable(true);
  this->list_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  this->fit_list_height();
  this->list_scroll->viewport()->setAutoFillBackground(false);
  this->list_scroll->setStyleSheet(
      QString(
          "QScrollArea#bakeNodeList { background: %1; border: 1px solid %2;"
          " border-radius: 9px; padding: 2px; }"
          "QScrollArea#bakeNodeList QScrollBar:vertical { background: transparent;"
          " width: 8px; margin: 6px 3px 6px 0px; }"
          "QScrollArea#bakeNodeList QScrollBar::handle:vertical { background: %3;"
          " border-radius: 2px; min-height: 28px; }"
          "QScrollArea#bakeNodeList QScrollBar::handle:vertical:hover { background: %4; }"
          "QScrollArea#bakeNodeList QScrollBar::add-line:vertical,"
          " QScrollArea#bakeNodeList QScrollBar::sub-line:vertical { height: 0px; }"
          "QScrollArea#bakeNodeList QScrollBar::add-page:vertical,"
          " QScrollArea#bakeNodeList QScrollBar::sub-page:vertical { background: none; }")
          .arg(mix(colors.bg_deep, colors.bg_primary, 0.55).name(),
               mix(colors.bg_primary, colors.border, 0.40).name(),
               mix(colors.bg_primary, colors.text_primary, 0.22).name(),
               mix(colors.bg_primary, colors.text_primary, 0.40).name()));
  body->addWidget(this->list_scroll);

  // --- buttons: cancel while it runs, done at the end
  this->cancel_button = this->add_button("Cancel bake", Role::Danger, false, true, false);
  QObject::connect(this->cancel_button,
                   &QPushButton::clicked,
                   this,
                   &BatchExportProgressDialog::on_cancel_clicked);

  // where the files went, once there are files
  this->open_button = this->add_button("Open folder",
                                       Role::Secondary,
                                       false,
                                       false,
                                       false);
  this->open_button->hide();
  QObject::connect(
      this->open_button,
      &QPushButton::clicked,
      this,
      [this]() { QDesktopServices::openUrl(QUrl::fromLocalFile(this->output_folder)); });

  this->done_button = this->add_button("Done", Role::Primary, true);
  this->done_button->hide(); // shown when the bake is over
}

} // namespace hesiod
