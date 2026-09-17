/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>

#include <QGuiApplication>
#include <QPushButton>
#include <QScreen>
#include <QScrollBar>
#include <QShowEvent>
#include <QVBoxLayout>

#include "hesiod/gui/widgets/scrollable_dialog.hpp"

namespace hesiod
{

namespace
{
/// Breathing room left between the dialog and the screen edges, logical px.
constexpr int kScreenMargin = 48;

/// Smallest the dialog may be clamped to; below this it is unusable anyway.
constexpr int kMinWidth = 320;
constexpr int kMinHeight = 220;
} // namespace

ScrollableDialog::ScrollableDialog(QWidget                          *content,
                                   const QString                    &title,
                                   QDialogButtonBox::StandardButtons button_set,
                                   QWidget                          *parent)
    : QDialog(parent), body(content)
{
  this->setWindowTitle(title);

  this->scroll = new QScrollArea(this);
  this->scroll->setWidgetResizable(true);
  this->scroll->setFrameShape(QFrame::NoFrame);

  // both on demand: a pane too wide for the screen has to stay pannable, and
  // clipping it is what makes a control unreachable at a large interface scale
  this->scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  this->scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

  if (this->body)
  {
    this->body->setParent(nullptr); // the scroll area reparents it
    this->scroll->setWidget(this->body);
  }

  // The scroll area's own size hint follows its widget, which would drag the
  // dialog back up to the content's full height. A small minimum is what lets
  // the user shrink the dialog and lets fit_to() clamp it.
  this->scroll->setMinimumSize(kMinWidth, kMinHeight);

  this->buttons = new QDialogButtonBox(button_set, this);
  if (QPushButton *ok = this->buttons->button(QDialogButtonBox::Ok))
    ok->setDefault(true);

  this->connect(this->buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  this->connect(this->buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  auto *layout = new QVBoxLayout(this);
  layout->addWidget(this->scroll, 1);
  layout->addWidget(this->buttons, 0); // outside the scroll area, always visible

  this->setSizeGripEnabled(true);
}

QRect ScrollableDialog::available_screen_rect() const
{
  const QScreen *screen = this->screen();

  if (!screen && this->parentWidget())
    screen = this->parentWidget()->screen();

  if (!screen)
    screen = QGuiApplication::primaryScreen();

  return screen ? screen->availableGeometry() : QRect(0, 0, 1024, 768);
}

QDialogButtonBox *ScrollableDialog::button_box() const { return this->buttons; }

QWidget *ScrollableDialog::content() const { return this->body; }

void ScrollableDialog::fit_to(const QRect &available)
{
  const int max_w = std::max(kMinWidth, available.width() - kScreenMargin);
  const int max_h = std::max(kMinHeight, available.height() - kScreenMargin);

  // what the dialog would like: the content's own width plus the frame, and
  // enough height for content and buttons
  int wanted_w = kMinWidth;
  int wanted_h = kMinHeight;

  if (this->body)
  {
    const QSize hint = this->body->sizeHint().expandedTo(
        this->body->minimumSizeHint());

    // room for the vertical scrollbar, so the content is not squeezed sideways
    // the moment it becomes tall enough to need one
    const int bar = this->scroll->verticalScrollBar()
                        ? this->scroll->verticalScrollBar()->sizeHint().width()
                        : 16;

    wanted_w = hint.width() + bar;
    wanted_h = hint.height();
  }

  wanted_h += this->buttons ? this->buttons->sizeHint().height() : 0;

  if (QLayout *l = this->layout())
  {
    const QMargins m = l->contentsMargins();
    wanted_w += m.left() + m.right();
    wanted_h += m.top() + m.bottom() + l->spacing();
  }

  const QSize size(std::clamp(wanted_w, kMinWidth, max_w),
                   std::clamp(wanted_h, kMinHeight, max_h));

  // never let a minimum size hold the dialog larger than the screen: that is
  // exactly the state where the buttons end up unreachable
  this->setMaximumSize(max_w, max_h);
  this->resize(size);

  QRect geom = this->geometry();
  geom.setSize(size);
  geom.moveCenter(available.center());
  this->setGeometry(geom);
}

QScrollArea *ScrollableDialog::scroll_area() const { return this->scroll; }

void ScrollableDialog::showEvent(QShowEvent *event)
{
  // fit once, on first show: doing it on every show would undo a size the user
  // dragged to
  if (!this->fitted)
  {
    this->fitted = true;
    this->fit_to(this->available_screen_rect());
  }

  QDialog::showEvent(event);
}

} // namespace hesiod
