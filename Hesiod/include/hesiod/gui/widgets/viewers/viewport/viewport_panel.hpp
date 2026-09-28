/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <vector>

#include <QAbstractButton>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariantAnimation>
#include <QWidget>

#include "meta_qt/container_widget.hpp"
#include "meta_qt/designs/industrial/panel_chrome.hpp"
#include "meta_qt/ui/theme.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/gui_utils.hpp"
#include "hesiod/gui/widgets/viewers/viewport/panel_model.hpp"
#include "hesiod/gui/widgets/viewers/viewport/sun_dome.hpp"
#include "hesiod/gui/widgets/viewers/viewport/viewport_specs.hpp"
#include "hesiod/gui/widgets/viewers/viewport/viewport_style.hpp"

namespace hesiod::viewport
{

// =====================================
// ViewportPanel
// =====================================

// A small icon button for the panel header (reset, close).
class HeaderButton final : public QAbstractButton
{
public:
  enum class Kind
  {
    Reset,
    Close
  };

  HeaderButton(Kind kind, QWidget *parent) : QAbstractButton(parent), kind(kind)
  {
    this->setFixedSize(28, 26);
    this->setCursor(Qt::PointingHandCursor);
    this->setAttribute(Qt::WA_Hover);
    this->setToolTip(kind == Kind::Reset ? "Reset this panel to defaults" : "Close");
  }

protected:
  void paintEvent(QPaintEvent *) override
  {
    const auto &colors = HSD_CTX.app_settings.colors;
    QPainter    p(this);
    p.setRenderHint(QPainter::Antialiasing);

    if (this->underMouse())
    {
      p.setPen(Qt::NoPen);
      p.setBrush(mix_colors(surface_color(), colors.text_primary, 0.08));
      p.drawRoundedRect(QRectF(this->rect()).adjusted(1, 1, -1, -1), 6, 6);
    }

    const QColor  ink = mix_colors(surface_color(),
                                  colors.text_primary,
                                  this->underMouse() ? 0.95 : 0.65);
    const QPointF c = QRectF(this->rect()).center();
    if (this->kind == Kind::Reset)
      paint_reset(p, c, 20, ink);
    else
    {
      QPen pen(ink, 1.4, Qt::SolidLine, Qt::RoundCap);
      p.setPen(pen);
      p.drawLine(c + QPointF(-4.5, -4.5), c + QPointF(4.5, 4.5));
      p.drawLine(c + QPointF(4.5, -4.5), c + QPointF(-4.5, 4.5));
    }
  }

private:
  Kind kind;
};

class ViewportPanel final : public QWidget
{
public:
  ViewportPanel(const QString &title, std::unique_ptr<PanelModel> model, QWidget *parent)
      : QWidget(parent), model(std::move(model))
  {
    this->setObjectName("hsdViewportPanel");
    this->setAttribute(Qt::WA_NoSystemBackground);
    this->setFixedWidth(kWidth);
    this->hide();

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(10, 8, 6, 10);
    root->setSpacing(6);

    // header: reset | title | close
    auto *header = new QHBoxLayout();
    header->setContentsMargins(0, 0, 4, 0);
    header->setSpacing(4);
    auto *reset = new HeaderButton(HeaderButton::Kind::Reset, this);
    this->reset_button = reset;
    header->addWidget(reset);
    auto *label = new QLabel(title, this);
    label->setObjectName("viewportPanelTitle");
    header->addWidget(label, 1);
    auto *close = new HeaderButton(HeaderButton::Kind::Close, this);
    header->addWidget(close);
    root->addLayout(header);

    this->scroll = new QScrollArea(this);
    this->scroll->setObjectName("viewportPanelScroll");
    this->scroll->setWidgetResizable(true);
    this->scroll->setFrameShape(QFrame::NoFrame);
    this->scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    this->scroll->viewport()->setAutoFillBackground(false);

    this->content = new QWidget();
    this->content->setObjectName("viewportPanelContent");
    this->content->setAutoFillBackground(false);
    this->content_layout = new QVBoxLayout(this->content);
    this->content_layout->setContentsMargins(0, 0, 4, 0);
    this->content_layout->setSpacing(8);
    this->scroll->setWidget(this->content);
    root->addWidget(this->scroll, 1);
    this->content->installEventFilter(this);

    const auto &colors = HSD_CTX.app_settings.colors;
    QString     css = QString(R"(
      QLabel#viewportPanelTitle { color: %1; font-size: 13px; font-weight: 600;
        background: transparent; padding-left: 4px; }
      QWidget#sunDome, QScrollArea#viewportPanelScroll, QWidget#viewportPanelContent {
        background: transparent; border: none; }
      QPushButton#viewportAction {
        background: %2; color: %1; border: 1px solid %3; border-radius: 7px;
        padding: 6px 12px; font-size: 12px; }
      QPushButton#viewportAction:hover { border-color: %4; }
      QWidget#viewportPreviewRows { background: transparent; }
      QWidget#viewportPreviewRows QLabel { color: %1; font-size: 12px; background: transparent; }
      QWidget#viewportPreviewRows QComboBox {
        background: %2; border: 1px solid %3; border-radius: 6px;
        padding: 3px 8px; min-height: 20px; font-size: 12px; }
      QWidget#viewportPreviewRows QComboBox:hover { border-color: %4; }
      QWidget#viewportPreviewRows QComboBox::drop-down { border: none; width: 18px; }
    )")
                      .arg(colors.text_primary.name(),
                           mix_colors(surface_color(), colors.text_primary, 0.06).name(),
                           border_color().name(),
                           colors.accent.name());
    this->setStyleSheet(css);
    this->scroll->setStyleSheet(
        meta::qt::industrial::scrollbar_stylesheet(viewport_theme()));

    this->anim = new QVariantAnimation(this);
    this->anim->setEasingCurve(QEasingCurve::OutCubic);
    QObject::connect(this->anim,
                     &QVariantAnimation::valueChanged,
                     this,
                     [this](const QVariant &v) { this->apply_progress(v.toReal()); });
    QObject::connect(this->anim,
                     &QVariantAnimation::finished,
                     this,
                     [this]()
                     {
                       if (this->closing)
                       {
                         this->hide();
                         this->closing = false;
                       }
                       // effects make every repaint an offscreen pass: drop it
                       this->setGraphicsEffect(nullptr);
                       this->effect = nullptr;
                     });

    // While open, follow the renderer: values also change behind the panel
    // (auto-rotating sun, the orientation gizmo, a project load...).
    this->follow = new QTimer(this);
    this->follow->setInterval(40);
    QObject::connect(this->follow,
                     &QTimer::timeout,
                     this,
                     [this]()
                     {
                       if (!this->isVisible())
                         this->follow->stop();
                       else if (this->model && !this->closing)
                         this->model->sync_from_renderer();
                     });

    QObject::connect(reset,
                     &QAbstractButton::clicked,
                     this,
                     [this]()
                     {
                       if (this->model)
                         this->model->reset_to_defaults();
                     });
    QObject::connect(close,
                     &QAbstractButton::clicked,
                     this,
                     [this]()
                     {
                       if (this->on_close)
                         this->on_close();
                     });
  }

  ~ViewportPanel() override
  {
    // rows subscribe to the model's attributes: remove them first
    delete this->scroll;
    this->scroll = nullptr;
  }

  PanelModel *get_model() const { return this->model.get(); }
  void        set_reset_visible(bool visible) { this->reset_button->setVisible(visible); }
  QVBoxLayout *body() const { return this->content_layout; }

  std::function<void()> on_close;

  // open next to the rail, from its side
  void popup(const QRect &rail, const QRect &item, Edge edge)
  {
    if (this->model)
      this->model->sync_from_renderer();

    const QRect vr = this->parentWidget()->rect().adjusted(kMargin,
                                                           kMargin,
                                                           -kMargin,
                                                           -kMargin);
    const int   gap = 8;

    // The panel keeps to the side of the rail, never over it: over it, a
    // second click on the tool would land on the panel instead of closing it.
    QRect area = vr;
    switch (edge)
    {
    case Edge::Right:
      area.setRight(rail.left() - gap - 1);
      break;
    case Edge::Left:
      area.setLeft(rail.right() + gap + 1);
      break;
    case Edge::Top:
      area.setTop(rail.bottom() + gap + 1);
      break;
    case Edge::Bottom:
      area.setBottom(rail.top() - gap - 1);
      break;
    }

    this->fit_area = area;
    this->fit_edge = edge;
    this->fit_item = item;

    this->slide_from = edge == Edge::Right  ? QPoint(12, 0)
                       : edge == Edge::Left ? QPoint(-12, 0)
                       : edge == Edge::Top  ? QPoint(0, -12)
                                            : QPoint(0, 12);
    this->closing = false;
    this->place(this->measured_height());
    this->move(this->home + this->slide_from);
    this->show();
    this->raise();
    this->run(1.0, 190);
    this->follow->start();

    // Rows only report their real height once shown, and sections animate
    // open and shut: follow the content (eventFilter) rather than measuring
    // once, so the panel grows and shrinks with it.
    this->fit_height();
  }

  void fit_height()
  {
    this->content->adjustSize();
    const int h = this->measured_height();
    if (h == this->height())
      return;
    this->place(h);
    this->move(this->home + this->slide_from * (1.0 - this->progress));
  }

  int measured_height() const
  {
    const int content_h = this->content->sizeHint().height() + 8 + 34 + 18;
    const int room = std::max(1, this->fit_area.height());
    return std::min(room, std::max(160, content_h));
  }

  // size to `h` and settle `home` inside the area beside the rail
  void place(int h)
  {
    const QRect &area = this->fit_area;
    const QRect &item = this->fit_item;
    this->setFixedHeight(h);

    QPoint pos;
    switch (this->fit_edge)
    {
    case Edge::Right:
      pos = QPoint(area.right() + 1 - kWidth, item.top() - 6);
      break;
    case Edge::Left:
      pos = QPoint(area.left(), item.top() - 6);
      break;
    case Edge::Top:
      pos = QPoint(item.left() - 6, area.top());
      break;
    case Edge::Bottom: // grows upwards from just above the rail
      pos = QPoint(item.left() - 6, area.bottom() + 1 - h);
      break;
    }
    pos.setX(std::clamp(pos.x(),
                        area.left(),
                        std::max(area.left(), area.right() + 1 - kWidth)));
    pos.setY(
        std::clamp(pos.y(), area.top(), std::max(area.top(), area.bottom() + 1 - h)));
    this->home = pos;
  }

  void dismiss(bool animate)
  {
    if (!this->isVisible())
      return;
    if (!animate || anim_ms(1) == 0)
    {
      this->anim->stop();
      this->setGraphicsEffect(nullptr);
      this->effect = nullptr;
      this->hide();
      return;
    }
    this->closing = true;
    this->run(0.0, 140);
  }

protected:
  void paintEvent(QPaintEvent *) override
  {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath shape;
    shape.addRoundedRect(QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5), 11, 11);
    p.fillPath(shape, surface_color());
    p.setPen(QPen(border_color(), 1));
    p.drawPath(shape);
  }

  // Scrolling past the end of the content arrives here (the scroll area
  // ignores it at its limits): stop it, or it would zoom the view below.
  void wheelEvent(QWheelEvent *event) override { event->accept(); }

  // Same for clicks anywhere on the panel that its rows leave unhandled: the
  // renderer would take the press as the start of a camera drag, and never
  // see a release handled by the row.
  void mousePressEvent(QMouseEvent *event) override { event->accept(); }
  void mouseReleaseEvent(QMouseEvent *event) override { event->accept(); }
  void mouseMoveEvent(QMouseEvent *event) override { event->accept(); }
  void mouseDoubleClickEvent(QMouseEvent *event) override { event->accept(); }

  void keyPressEvent(QKeyEvent *event) override
  {
    if (event->key() == Qt::Key_Escape && this->on_close)
    {
      this->on_close();
      return;
    }
    QWidget::keyPressEvent(event);
  }

private:
  static constexpr int kWidth = 400;

  void run(qreal to, int ms)
  {
    if (!this->effect)
    {
      this->effect = new QGraphicsOpacityEffect(this);
      this->effect->setOpacity(this->progress);
      this->setGraphicsEffect(this->effect);
    }
    this->anim->stop();
    this->anim->setStartValue(this->progress);
    this->anim->setEndValue(to);
    this->anim->setDuration(anim_ms(ms));
    this->anim->start();
    if (anim_ms(ms) == 0)
      this->apply_progress(to);
  }

  void apply_progress(qreal t)
  {
    this->progress = t;
    if (this->effect)
      this->effect->setOpacity(t);
    this->move(this->home + this->slide_from * (1.0 - t));
  }

  bool eventFilter(QObject *watched, QEvent *event) override
  {
    if (watched == this->content && this->isVisible() &&
        (event->type() == QEvent::LayoutRequest || event->type() == QEvent::Resize) &&
        !this->fit_pending)
    {
      // coalesce: a section animating fires one of these per frame
      this->fit_pending = true;
      QTimer::singleShot(0,
                         this,
                         [this]()
                         {
                           this->fit_pending = false;
                           this->fit_height();
                         });
    }
    return QWidget::eventFilter(watched, event);
  }

  std::unique_ptr<PanelModel> model;
  QAbstractButton            *reset_button = nullptr;
  QScrollArea                *scroll = nullptr;
  QWidget                    *content = nullptr;
  QVBoxLayout                *content_layout = nullptr;

  // placement of the last popup, for re-fitting as the content changes
  QRect fit_area; // viewport part beside the rail
  Edge  fit_edge = Edge::Right;
  QRect fit_item;
  bool  fit_pending = false;

  QTimer                 *follow = nullptr; // syncs from the renderer while open
  QVariantAnimation      *anim = nullptr;
  QGraphicsOpacityEffect *effect = nullptr;
  qreal                   progress = 0.0;
  bool                    closing = false;
  QPoint                  home;
  QPoint                  slide_from;
};

} // namespace hesiod::viewport
