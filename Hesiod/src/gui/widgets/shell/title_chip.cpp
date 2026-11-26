/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <cmath>

#include <QEnterEvent>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QLineEdit>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>

#include "meta_qt/ui/theme.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/shell/title_chip.hpp"
#include "hesiod/gui/widgets/utils/gui_utils.hpp"

namespace hesiod
{

namespace
{

// lengths, in pixels
constexpr qreal kPadLeft = 14.0;
constexpr qreal kPadRight = 11.0;
constexpr qreal kGap = 9.0;        // between the name, the note and the mark
constexpr qreal kChevron = 18.0;   // its gap and the glyph
constexpr qreal kMinWidth = 168.0; // a short name still reads as the title
constexpr qreal kEditWidth = 250.0;
constexpr int   kSavedNoteMs = 1500;

qreal unit(const Anim &anim) { return std::clamp(anim.value(), 0.0, 1.0); }

} // namespace

TitleChip::TitleChip(QWidget *parent) : QWidget(parent)
{
  this->setCursor(Qt::PointingHandCursor);
  this->setAttribute(Qt::WA_Hover);

  // A plain QWidget picks up the application stylesheet's background, filled
  // as a square behind the rounded chip: opt out, the chip paints itself.
  this->setObjectName("hsdTitleChip");
  this->setAttribute(Qt::WA_NoSystemBackground);
  this->setAutoFillBackground(false);

  this->editor = new QLineEdit(this);
  this->editor->setObjectName("hsdTitleEditor");
  this->editor->setAlignment(Qt::AlignCenter);
  this->editor->setFrame(false);
  this->editor->hide();
  this->editor->installEventFilter(this);

  QObject::connect(this->editor,
                   &QLineEdit::returnPressed,
                   this,
                   [this]() { this->finish_rename(true); });

  // the chip grows and shrinks with what is typed
  QObject::connect(this->editor,
                   &QLineEdit::textChanged,
                   this,
                   [this]()
                   {
                     if (this->editing)
                       this->retarget(true);
                   });

  this->saved_note.setSingleShot(true);
  QObject::connect(&this->saved_note,
                   &QTimer::timeout,
                   this,
                   [this]()
                   {
                     this->flashing = false;
                     this->retarget(true);
                   });

  // what moves the content re-lays the chip out, the rest only repaints
  for (Anim *anim : {&this->name_w, &this->word_w, &this->mark_t, &this->edit_t})
    anim->on_tick = [this]() { this->relayout(); };

  for (Anim *anim : {&this->name_t,
                     &this->word_t,
                     &this->check_t,
                     &this->hover_t,
                     &this->press_t,
                     &this->open_t})
    anim->on_tick = [this]() { this->update(); };

  // the tick goes with the mark; the next mark starts as a dot again
  this->mark_t.on_done = [this]()
  {
    if (this->mark_t.value() < 0.01 && !this->flashing)
      this->check_t.set(0.0);
  };

  this->retarget(false);
}

void TitleChip::animate_open(bool open)
{
  this->open_t.to(open ? 1.0 : 0.0,
                  open ? motion::popup_in : motion::popup_out,
                  motion::change());
}

void TitleChip::begin_rename(const QString                       &current,
                             std::function<void(const QString &)> commit)
{
  this->on_commit = std::move(commit);
  this->editor->setText(current);
  this->editing = true;
  this->retarget(true);
  this->place_editor();
  this->editor->show();
  this->editor->selectAll();
  this->editor->setFocus(Qt::OtherFocusReason);
  this->update();
}

qreal TitleChip::content() const
{
  const qreal show = 1.0 - unit(this->edit_t);
  return this->name_w.value() +
         (this->word_w.value() + this->mark_slot() + kChevron) * show;
}

void TitleChip::enterEvent(QEnterEvent *event)
{
  this->hovered = true;
  this->hover_t.to(1.0, motion::hover_in, motion::enter());
  QWidget::enterEvent(event);
}

bool TitleChip::eventFilter(QObject *watched, QEvent *event)
{
  if (watched == this->editor)
  {
    if (event->type() == QEvent::KeyPress &&
        static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape)
    {
      this->finish_rename(false);
      return true;
    }
    if (event->type() == QEvent::FocusOut && this->editing)
      this->finish_rename(false); // clicking away cancels, Enter commits
  }
  return QWidget::eventFilter(watched, event);
}

void TitleChip::finish_rename(bool commit)
{
  if (!this->editing)
    return;

  const QString text = this->editor->text().trimmed();
  this->editing = false;
  this->editor->hide();
  this->retarget(true);

  auto callback = std::move(this->on_commit);
  this->on_commit = nullptr;
  if (commit && callback)
  {
    this->committing = true;
    callback(text);
    this->committing = false;
  }
}

void TitleChip::flash_saved()
{
  if (this->dirty || !this->isVisible())
    return;

  this->flashing = true;
  this->saved_note.start(kSavedNoteMs);
  this->retarget(true);
}

QFont TitleChip::hint_font() const { return meta::qt::ui_font(11); }

void TitleChip::leaveEvent(QEvent *event)
{
  this->hovered = false;
  this->hover_t.to(0.0, motion::hover_out, motion::leave());
  if (!this->pressed)
    this->press_t.to(0.0, motion::press_out, motion::enter());
  QWidget::leaveEvent(event);
}

qreal TitleChip::mark_size() const { return 6.0 + 3.0 * unit(this->check_t); }

qreal TitleChip::mark_slot() const
{
  return (kGap + this->mark_size()) * unit(this->mark_t);
}

// a quick second click is a click too (it reopens the menu the first one
// closed), not a rename
void TitleChip::mouseDoubleClickEvent(QMouseEvent *event)
{
  this->mousePressEvent(event);
}

// The menu opens on press, like a menu bar item: waiting for the release, or
// for a possible double-click, made it feel as if it lagged behind. Rename is
// the menu's first item (and F2).
void TitleChip::mousePressEvent(QMouseEvent *event)
{
  if (event->button() != Qt::LeftButton || this->editing || !this->on_clicked)
    return QWidget::mousePressEvent(event);

  event->accept();

  // A click on the chip while its menu is open only closes the menu: the menu
  // is built with setNoReplayFor(chip) (see TitleBar::title_chip), so Qt does
  // not replay that press here and open it straight again.
  this->pressed = true;
  this->press_t.to(1.0, motion::press_in, motion::enter());
  this->animate_open(true);
  this->repaint();

  // The menu runs its own event loop, so anything can happen to the chip
  // before it returns. The callback is called through a copy, and the chip is
  // not touched again if it was destroyed meanwhile.
  const QPointer<TitleChip>                 guard(this);
  const std::function<void(const QPoint &)> clicked = this->on_clicked;
  clicked(this->mapToGlobal(QPoint(this->width() / 2, this->height() + 4)));
  if (!guard)
    return;

  this->pressed = false;
  this->press_t.to(0.0, motion::press_out, motion::enter());
  this->animate_open(false);
  this->hovered = this->rect().contains(this->mapFromGlobal(QCursor::pos()));
  if (!this->hovered)
    this->hover_t.to(0.0, motion::hover_out, motion::leave());
  this->update();
}

void TitleChip::mouseReleaseEvent(QMouseEvent *event)
{
  this->pressed = false;
  this->press_t.to(0.0, motion::press_out, motion::enter());
  QWidget::mouseReleaseEvent(event);
}

QFont TitleChip::name_font() const { return meta::qt::ui_font(12); }

qreal TitleChip::name_room() const
{
  const qreal others = this->editing ? 2.0 * kPadLeft
                                     : kPadLeft + this->word_w.target() +
                                           (kGap + 9.0) * this->mark_t.target() +
                                           kChevron + kPadRight;
  return std::max(24.0, this->max_width - others);
}

qreal TitleChip::natural() const
{
  const qreal edit = unit(this->edit_t);
  const qreal pad_right = kPadRight + (kPadLeft - kPadRight) * edit;
  const qreal least = kMinWidth + (kEditWidth - kMinWidth) * edit;
  return std::max(least, kPadLeft + this->content() + pad_right);
}

int TitleChip::natural_width() const
{
  return static_cast<int>(std::ceil(this->natural()));
}

void TitleChip::paintEvent(QPaintEvent *)
{
  const auto &colors = HSD_CTX.app_settings.colors;

  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  const qreal hover = unit(this->hover_t);
  const qreal held = unit(this->press_t);
  const qreal edit = unit(this->edit_t);
  const qreal show = 1.0 - edit; // everything but the name steps back for the editor
  const Parts at = this->parts();
  const qreal h = this->height();

  // the plate: lifts under the pointer, and becomes a field, sunk with an
  // accent rim, while the name is being edited
  {
    const QRectF box = QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5);

    QColor fill = mix_colors(colors.bg_primary,
                             colors.text_primary,
                             std::max(0.05 * hover, 0.10 * held));
    fill = mix_colors(fill, colors.bg_deep, edit);

    QColor rim = mix_colors(panel_border_color(),
                            mix_colors(colors.bg_primary, colors.border, 0.75),
                            hover);
    rim = mix_colors(rim, colors.accent, edit);

    p.setPen(QPen(rim, 1));
    p.setBrush(fill);
    p.drawRoundedRect(box, 8, 8);
  }

  // the name; the editor stands exactly here while it is open. A new name
  // fades in over the old one as the slot takes its width.
  if (!this->editing)
  {
    const QFontMetricsF fm(this->name_font());
    const qreal         swap = unit(this->name_t);
    const int           room = static_cast<int>(std::ceil(this->name_room())) + 1;

    p.save();
    p.setClipRect(QRectF(at.name_x - 1.0, 0.0, at.name_w + 2.0, h));
    p.setFont(this->name_font());

    const auto draw = [&](const QString &text, qreal alpha)
    {
      if (alpha < 0.004 || text.isEmpty())
        return;
      QColor ink = colors.text_primary;
      ink.setAlphaF(alpha);
      p.setPen(ink);
      p.drawText(QRectF(at.name_x, 0.0, this->max_width, h),
                 Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
                 fm.elidedText(text, Qt::ElideMiddle, room));
    };

    draw(this->prev_name, 1.0 - swap);
    draw(this->name, swap);
    p.restore();
  }

  // "Not saved" / "Saved"
  if (show > 0.004 && at.word_w > 0.5)
  {
    const qreal  swap = unit(this->word_t);
    const QColor dim = mix_colors(colors.bg_primary, colors.text_primary, 0.50);

    p.save();
    p.setClipRect(QRectF(at.word_x, 0.0, at.word_w + 1.0, h));
    p.setFont(this->hint_font());

    const auto draw = [&](const QString &text, qreal alpha)
    {
      if (alpha < 0.004 || text.isEmpty())
        return;
      QColor ink = dim;
      ink.setAlphaF(alpha);
      p.setPen(ink);
      p.drawText(QRectF(at.word_x + kGap, 0.0, this->max_width, h),
                 Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
                 text);
    };

    draw(this->prev_word, (1.0 - swap) * show);
    draw(this->word, swap * show);
    p.restore();
  }

  // unsaved changes: a dot. Once they are saved it turns into a tick.
  const qreal mark = unit(this->mark_t) * show;
  if (mark > 0.004)
  {
    const qreal   tick = unit(this->check_t);
    const QPointF c(at.mark_x + at.mark_w - this->mark_size() / 2.0, h / 2.0);

    QColor ink = colors.accent;
    ink.setAlphaF(mark);

    if (tick < 0.996)
    {
      const qreal r = 3.0 * mark * (1.0 - tick);
      p.setPen(Qt::NoPen);
      p.setBrush(ink);
      p.drawEllipse(c, r, r);
    }

    if (tick > 0.004)
    {
      // drawn from its short stroke on, as a pen would
      const QPointF a = c + QPointF(-3.8, 0.2);
      const QPointF b = c + QPointF(-1.3, 2.7);
      const QPointF d = c + QPointF(3.8, -2.7);
      const qreal   first = QLineF(a, b).length();
      const qreal   whole = first + QLineF(b, d).length();
      const qreal   drawn = whole * tick;

      QPainterPath stroke(a);
      if (drawn <= first)
        stroke.lineTo(a + (b - a) * (drawn / first));
      else
      {
        stroke.lineTo(b);
        stroke.lineTo(b + (d - b) * ((drawn - first) / (whole - first)));
      }

      QPen pen(ink, 1.6);
      pen.setCapStyle(Qt::RoundCap);
      pen.setJoinStyle(Qt::RoundJoin);
      p.setPen(pen);
      p.setBrush(Qt::NoBrush);
      p.drawPath(stroke);
    }
  }

  // the menu affordance: a chevron that turns to point up while the menu is
  // open
  if (show > 0.004)
  {
    QColor ink = mix_colors(colors.bg_primary, colors.text_primary, 0.50 + 0.40 * hover);
    ink.setAlphaF(show);

    QPen pen(ink, 1.5);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    p.save();
    p.translate(at.chevron_x + kChevron - 4.5, h / 2.0);
    p.rotate(180.0 * unit(this->open_t));
    QPainterPath chevron;
    chevron.moveTo(-3.5, -1.75);
    chevron.lineTo(0.0, 1.75);
    chevron.lineTo(3.5, -1.75);
    p.drawPath(chevron);
    p.restore();
  }
}

// the group sits in the middle of the chip
TitleChip::Parts TitleChip::parts() const
{
  const qreal edit = unit(this->edit_t);
  const qreal show = 1.0 - edit;
  const qreal pad_right = kPadRight + (kPadLeft - kPadRight) * edit;
  const qreal group = kPadLeft + this->content() + pad_right;

  Parts at{};
  at.name_x = (this->width() - group) / 2.0 + kPadLeft;
  at.name_w = this->name_w.value();
  at.word_x = at.name_x + at.name_w;
  at.word_w = this->word_w.value() * show;
  at.mark_x = at.word_x + at.word_w;
  at.mark_w = this->mark_slot() * show;
  at.chevron_x = at.mark_x + at.mark_w;
  return at;
}

// The editor is centred on the name's slot, so the text does not move when it
// takes over from the painted name or hands back to it.
void TitleChip::place_editor()
{
  if (!this->editing)
    return;

  const Parts at = this->parts();
  const qreal centre = at.name_x + at.name_w / 2.0;
  const qreal half = std::max(20.0, std::min(centre - 6.0, this->width() - 6.0 - centre));
  this->editor->setGeometry(static_cast<int>(std::round(centre - half)),
                            2,
                            static_cast<int>(std::round(2.0 * half)),
                            this->height() - 4);
}

void TitleChip::relayout()
{
  if (this->on_layout)
    this->on_layout();
  this->place_editor();
  this->update();
}

void TitleChip::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  this->place_editor();
}

void TitleChip::retarget(bool animate)
{
  const QString want_word = this->flashing ? QString("Saved")
                            : this->saved  ? QString()
                                           : QString("Not saved");
  const bool    want_mark = this->flashing || this->dirty;

  if (want_word != this->word)
  {
    this->prev_word = this->word;
    this->word = want_word;
    this->word_t.set(animate ? 0.0 : 1.0);
    if (animate)
      this->word_t.to(1.0, motion::value, motion::change());
  }

  const auto glide = [animate](Anim &anim, double target, int ms)
  {
    if (animate)
      anim.to(target, ms, motion::change());
    else
      anim.set(target);
  };

  const QFontMetricsF fm(this->name_font());
  const QFontMetricsF fh(this->hint_font());

  glide(this->edit_t, this->editing ? 1.0 : 0.0, motion::section_out);
  glide(this->word_w,
        this->word.isEmpty() ? 0.0 : kGap + fh.horizontalAdvance(this->word),
        motion::value);
  glide(this->mark_t,
        want_mark ? 1.0 : 0.0,
        want_mark ? motion::section_out : motion::value);

  if (this->flashing)
    glide(this->check_t, 1.0, motion::section_in);
  else if (want_mark || !animate)
    glide(this->check_t, 0.0, motion::section_out);

  // after the others: the room left for the name depends on them
  const qreal wanted = this->editing ? fm.horizontalAdvance(this->editor->text()) + 10.0
                                     : fm.horizontalAdvance(this->name);
  glide(this->name_w,
        std::min(wanted, this->name_room()),
        this->editing ? motion::hover_in : motion::value);

  if (!animate)
    this->relayout();
}

void TitleChip::set(const QString &new_name,
                    const QString &new_path,
                    bool           new_dirty,
                    bool           new_saved)
{
  const QString shown = new_name.isEmpty() ? QString("Untitled") : new_name;
  const bool    animate = this->isVisible();

  if (shown != this->name)
  {
    // a name the user has just typed is already what they are looking at
    if (animate && !this->committing)
    {
      this->prev_name = this->name;
      this->name_t.set(0.0);
      this->name_t.to(1.0, motion::value, motion::change());
    }
    this->name = shown;
  }

  // "Saved" is about the project that was saved, not the one opened next
  if (new_dirty ||
      (new_path != this->path && !this->path.isEmpty() && !new_path.isEmpty()))
  {
    this->flashing = false;
    this->saved_note.stop();
  }

  this->path = new_path;
  this->dirty = new_dirty;
  this->saved = new_saved;

  this->setToolTip(new_saved
                       ? new_path + "\n\nClick for project actions (F2 renames)"
                       : QString("This project has no file yet.\n\nClick for project "
                                 "actions (F2 renames)"));
  this->retarget(animate);
}

void TitleChip::set_max_width(int width)
{
  if (width == this->max_width)
    return;
  this->max_width = width;
  this->retarget(false);
}

} // namespace hesiod
