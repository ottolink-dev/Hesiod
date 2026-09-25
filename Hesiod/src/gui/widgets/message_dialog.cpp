/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>

#include <QApplication>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScreen>
#include <QStyle>
#include <QVBoxLayout>

#include "meta_qt/ui/theme.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/message_dialog.hpp"

// last: windows.h leaks macros (interface, min, max...) that clash with the
// project headers above
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <dwmapi.h>
#undef interface
#endif

namespace hesiod
{

namespace
{

constexpr int kShadow = 18; // transparent margin holding the painted shadow
constexpr int kRadius = 12;
constexpr int kWidth = 460; // card width

const QColor kWarning("#d9a441");
const QColor kDanger("#e06c62");

QColor mix(const QColor &from, const QColor &to, qreal amount)
{
  amount = std::clamp(amount, 0.0, 1.0);
  return QColor::fromRgbF(from.redF() + (to.redF() - from.redF()) * amount,
                          from.greenF() + (to.greenF() - from.greenF()) * amount,
                          from.blueF() + (to.blueF() - from.blueF()) * amount);
}

QColor surface()
{
  const auto &c = HSD_CTX.app_settings.colors;
  return mix(c.bg_deep, c.bg_primary, 0.55);
}

QColor border()
{
  const auto &c = HSD_CTX.app_settings.colors;
  return mix(c.bg_primary, c.border, 0.45);
}

QColor kind_color(MessageDialog::Kind kind)
{
  switch (kind)
  {
  case MessageDialog::Kind::Warning:
    return kWarning;
  case MessageDialog::Kind::Error:
    return kDanger;
  default:
    return HSD_CTX.app_settings.colors.accent;
  }
}

MessageDialog::Kind kind_from_icon(QMessageBox::Icon icon)
{
  switch (icon)
  {
  case QMessageBox::Warning:
    return MessageDialog::Kind::Warning;
  case QMessageBox::Critical:
    return MessageDialog::Kind::Error;
  case QMessageBox::Question:
    return MessageDialog::Kind::Question;
  default:
    return MessageDialog::Kind::Info;
  }
}

} // namespace

// =====================================
// MessageDialog
// =====================================

QPixmap MessageDialog::badge(Kind kind, int size, qreal dpr)
{
  QPixmap pixmap(QSize(size, size) * dpr);
  pixmap.setDevicePixelRatio(dpr);
  pixmap.fill(Qt::transparent);

  const QColor color = kind_color(kind);

  QPainter p(&pixmap);
  p.setRenderHint(QPainter::Antialiasing);

  const QRectF disc(1.0, 1.0, size - 2.0, size - 2.0);
  QColor       fill = color;
  fill.setAlphaF(0.16);
  p.setPen(QPen(mix(surface(), color, 0.55), 1.0));
  p.setBrush(fill);
  p.drawEllipse(disc);

  const QPointF c = disc.center();
  const qreal   s = size / 40.0; // glyphs are drawn on a 40 px design grid

  if (kind == Kind::Restore)
  {
    // open circular arrow
    QPen pen(color, 2.2 * s, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    const QRectF ring(c.x() - 8 * s, c.y() - 8 * s, 16 * s, 16 * s);
    p.drawArc(ring, 110 * 16, 290 * 16);

    QPainterPath  head;
    const QPointF tip(c.x() - 7.6 * s, c.y() - 3.2 * s);
    head.moveTo(tip + QPointF(-3.6 * s, -3.8 * s));
    head.lineTo(tip);
    head.lineTo(tip + QPointF(4.2 * s, -1.6 * s));
    p.drawPath(head);
    return pixmap;
  }

  if (kind == Kind::Error)
  {
    QPen pen(color, 2.4 * s, Qt::SolidLine, Qt::RoundCap);
    p.setPen(pen);
    p.drawLine(c + QPointF(-6 * s, -6 * s), c + QPointF(6 * s, 6 * s));
    p.drawLine(c + QPointF(6 * s, -6 * s), c + QPointF(-6 * s, 6 * s));
    return pixmap;
  }

  // "?", "!" and "i" come from the font, but centred on their outline rather
  // than on the text baseline, so they sit exactly in the middle of the disc
  const QString glyph = kind == Kind::Question ? "?" : kind == Kind::Warning ? "!" : "i";
  QFont         font = meta::qt::ui_font(qRound(20 * s), true);
  QPainterPath  path;
  path.addText(0, 0, font, glyph);
  const QRectF bounds = path.boundingRect();
  path.translate(c - bounds.center());

  p.setPen(Qt::NoPen);
  p.setBrush(color);
  p.drawPath(path);
  return pixmap;
}

MessageDialog::MessageDialog(QWidget       *parent,
                             Kind           kind,
                             const QString &title,
                             const QString &text)
    : QDialog(parent)
{
  const auto &colors = HSD_CTX.app_settings.colors;

  this->setObjectName("hsdMessage");
  this->setWindowTitle(title);
  this->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
  this->setAttribute(Qt::WA_TranslucentBackground);
  this->setModal(true);

  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(kShadow + 24, kShadow + 22, kShadow + 24, kShadow + 20);
  root->setSpacing(16);

  // --- badge + text
  auto *top = new QHBoxLayout();
  top->setSpacing(16);

  this->icon = new QLabel(this);
  this->icon->setFixedSize(40, 40);
  if (kind == Kind::None)
    this->icon->hide();
  else
    this->icon->setPixmap(MessageDialog::badge(kind, 40, this->devicePixelRatioF()));
  top->addWidget(this->icon, 0, Qt::AlignTop);

  this->text_layout = new QVBoxLayout();
  this->text_layout->setSpacing(6);

  auto *title_label = new QLabel(title, this);
  title_label->setObjectName("messageTitle");
  title_label->setWordWrap(true);
  this->text_layout->addWidget(title_label);

  if (!text.isEmpty())
  {
    auto *text_label = new QLabel(text, this);
    text_label->setObjectName("messageText");
    text_label->setWordWrap(true);
    this->text_layout->addWidget(text_label);
  }

  top->addLayout(this->text_layout, 1);
  root->addLayout(top);

  // --- custom content
  this->body_layout = new QVBoxLayout();
  this->body_layout->setContentsMargins(0, 0, 0, 0);
  this->body_layout->setSpacing(8);
  root->addLayout(this->body_layout);

  // --- details card (hidden until used)
  this->details_card = new QFrame(this);
  this->details_card->setObjectName("messageDetails");
  this->details = new QGridLayout(this->details_card);
  this->details->setContentsMargins(14, 10, 14, 10);
  this->details->setHorizontalSpacing(16);
  this->details->setVerticalSpacing(6);
  this->details->setColumnStretch(1, 1);
  this->details_card->hide();
  root->addWidget(this->details_card);

  this->footnote = new QLabel(this);
  this->footnote->setObjectName("messageFootnote");
  this->footnote->setWordWrap(true);
  this->footnote->hide();
  root->addWidget(this->footnote);

  // --- buttons: [danger]  ...  [secondary] [primary]
  auto *buttons = new QHBoxLayout();
  buttons->setSpacing(8);
  this->danger_layout = new QHBoxLayout();
  this->danger_layout->setSpacing(8);
  this->buttons_layout = new QHBoxLayout();
  this->buttons_layout->setSpacing(8);
  buttons->addLayout(this->danger_layout);
  buttons->addStretch(1);
  buttons->addLayout(this->buttons_layout);
  root->addSpacing(4);
  root->addLayout(buttons);

  this->set_card_width(kWidth);

  QString css = R"(
    QDialog#hsdMessage QWidget { background: transparent; color: INK; }
    QDialog#hsdMessage QLabel#messageTitle { font-size: 16px; font-weight: 600; }
    QDialog#hsdMessage QLabel#messageText { color: DIM; font-size: 13px; }
    QDialog#hsdMessage QLabel#messageFootnote { color: FAINT; font-size: 12px; }
    QDialog#hsdMessage QFrame#messageDetails {
      background: DEEP; border: 1px solid BORDER; border-radius: 8px; }
    QDialog#hsdMessage QLabel#detailKey { color: FAINT; font-size: 12px; }
    QDialog#hsdMessage QLabel#detailValue { color: INK; font-size: 12px; }
    QDialog#hsdMessage QPushButton {
      background: FIELD; color: INK; border: 1px solid BORDER; border-radius: 7px;
      padding: 7px 18px; font-size: 13px; min-width: 64px; }
    QDialog#hsdMessage QPushButton:hover { background: FIELD_HOVER; border-color: FAINT; }
    QDialog#hsdMessage QPushButton:focus { border-color: ACCENT; }
    QDialog#hsdMessage QPushButton[hsdRole="primary"] {
      background: ACCENT; border-color: ACCENT; color: #ffffff; font-weight: 600; }
    QDialog#hsdMessage QPushButton[hsdRole="primary"]:hover { background: ACCENT_HOVER; }
    QDialog#hsdMessage QPushButton[hsdRole="danger"] {
      background: transparent; color: DANGER; border-color: transparent; }
    QDialog#hsdMessage QPushButton[hsdRole="danger"]:hover {
      background: DANGER_SOFT; border-color: DANGER_BORDER; }
  )";

  const QColor                          s = surface();
  const std::pair<const char *, QColor> tokens[] = {
      {"ACCENT_HOVER", colors.accent.lighter(112)},
      {"ACCENT", colors.accent},
      {"DANGER_SOFT", mix(s, kDanger, 0.12)},
      {"DANGER_BORDER", mix(s, kDanger, 0.35)},
      {"DANGER", kDanger},
      {"FIELD_HOVER", mix(colors.bg_primary, colors.border, 0.30)},
      {"FIELD", colors.bg_primary},
      {"BORDER", border()},
      {"DEEP", colors.bg_deep},
      {"FAINT", mix(colors.bg_primary, colors.text_primary, 0.45)},
      {"DIM", mix(colors.bg_primary, colors.text_primary, 0.70)},
      {"INK", colors.text_primary}};
  for (const auto &[key, value] : tokens)
    css.replace(key, value.name());

  const QString family = meta::qt::ui_font(13).family();
  if (!family.isEmpty())
    css.prepend(
        QString("QDialog#hsdMessage QWidget { font-family: \"%1\"; }\n").arg(family));

  this->setStyleSheet(css);
}

void MessageDialog::add_detail(const QString &key, const QString &value)
{
  const int row = this->details->rowCount();

  auto *key_label = new QLabel(key, this->details_card);
  key_label->setObjectName("detailKey");
  this->details->addWidget(key_label, row, 0, Qt::AlignTop);

  auto *value_label = new QLabel(this->details_card);
  value_label->setObjectName("detailValue");
  value_label->setTextInteractionFlags(Qt::TextSelectableByMouse);

  // long paths are elided in the middle, where they are least informative
  const QFontMetrics fm(meta::qt::ui_font(12));
  value_label->setText(fm.elidedText(value, Qt::ElideMiddle, this->card_width - 150));
  if (value_label->text() != value)
    value_label->setToolTip(value);
  this->details->addWidget(value_label, row, 1);

  this->details_card->show();
}

QPushButton *MessageDialog::add_button(const QString &label,
                                       Role           role,
                                       bool           is_default,
                                       bool           is_escape,
                                       bool           closes)
{
  auto *button = new QPushButton(label, this);
  button->setCursor(Qt::PointingHandCursor);
  button->setAutoDefault(false);
  button->setProperty("hsdRole",
                      role == Role::Primary  ? "primary"
                      : role == Role::Danger ? "danger"
                                             : "secondary");

  if (role == Role::Danger || !closes)
    this->danger_layout->addWidget(button);
  else
    this->buttons_layout->addWidget(button);

  if (is_default)
  {
    button->setDefault(true);
    button->setFocus();
  }
  if (is_escape)
    this->escape = button;

  if (closes)
    QObject::connect(button,
                     &QPushButton::clicked,
                     this,
                     [this, button]()
                     {
                       this->clicked = button;
                       if (button == this->escape)
                         this->reject();
                       else
                         this->accept();
                     });
  return button;
}

void MessageDialog::set_card_width(int width)
{
  this->card_width = width;
  this->setFixedWidth(width + 2 * kShadow);
}

void MessageDialog::set_footnote(const QString &text)
{
  this->footnote->setText(text);
  this->footnote->setVisible(!text.isEmpty());
}

void MessageDialog::keyPressEvent(QKeyEvent *event)
{
  if (event->key() == Qt::Key_Escape)
  {
    if (this->escape)
      this->escape->click();
    else
      this->reject();
    return;
  }
  QDialog::keyPressEvent(event);
}

void MessageDialog::mousePressEvent(QMouseEvent *event)
{
  if (event->button() == Qt::LeftButton)
  {
    this->dragging = true;
    this->drag_offset = event->globalPosition().toPoint() -
                        this->frameGeometry().topLeft();
    event->accept();
    return;
  }
  QDialog::mousePressEvent(event);
}

void MessageDialog::mouseMoveEvent(QMouseEvent *event)
{
  if (this->dragging && (event->buttons() & Qt::LeftButton))
  {
    this->move(event->globalPosition().toPoint() - this->drag_offset);
    event->accept();
    return;
  }
  QDialog::mouseMoveEvent(event);
}

void MessageDialog::mouseReleaseEvent(QMouseEvent *event)
{
  this->dragging = false;
  QDialog::mouseReleaseEvent(event);
}

void MessageDialog::paintEvent(QPaintEvent *)
{
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  const QRectF card = QRectF(this->rect()).adjusted(kShadow, kShadow, -kShadow, -kShadow);

  // soft shadow: stacked rounded outlines fading out, offset slightly down
  p.setPen(Qt::NoPen);
  for (int i = kShadow; i > 0; i -= 2)
  {
    const qreal t = 1.0 - qreal(i) / kShadow;
    p.setBrush(QColor(0, 0, 0, qRound(22 * t * t)));
    p.drawRoundedRect(card.adjusted(-i, -i + 4, i, i + 4), kRadius + i, kRadius + i);
  }

  QPainterPath shape;
  shape.addRoundedRect(card.adjusted(0.5, 0.5, -0.5, -0.5), kRadius, kRadius);
  p.fillPath(shape, surface());
  p.setPen(QPen(border(), 1));
  p.setBrush(Qt::NoBrush);
  p.drawPath(shape);
}

void MessageDialog::showEvent(QShowEvent *event)
{
  QDialog::showEvent(event);

  // centre over the parent window (the title bar of a frameless dialog is not
  // there to be dragged back if it opens half off-screen)
  this->adjustSize();
  const QWidget *anchor = this->parentWidget() ? this->parentWidget()->window() : nullptr;
  QScreen       *screen = anchor ? anchor->screen() : QGuiApplication::primaryScreen();
  const QRect    area = anchor && anchor->isVisible() ? anchor->geometry()
                                                      : screen->availableGeometry();
  QRect          geom(QPoint(0, 0), this->size());
  geom.moveCenter(area.center());
  this->move(geom.topLeft());
}

// =====================================
// native dialog polish
// =====================================

namespace
{

class NativeDialogPolish final : public QObject
{
public:
  using QObject::QObject;

protected:
  bool eventFilter(QObject *watched, QEvent *event) override
  {
    if (event->type() != QEvent::Show)
      return false;

    auto *widget = qobject_cast<QWidget *>(watched);
    if (!widget || !widget->isWindow())
      return false;

    if (auto *box = qobject_cast<QMessageBox *>(widget))
      polish_message_box(box);

#ifdef Q_OS_WIN
    const Qt::WindowType type = widget->windowType();
    const bool           framed = !(widget->windowFlags() & Qt::FramelessWindowHint) &&
                        (type == Qt::Window || type == Qt::Dialog);

    if (framed && !widget->property("_hsd_caption").toBool())
    {
      widget->setProperty("_hsd_caption", true);

      const auto &colors = HSD_CTX.app_settings.colors;
      HWND        hwnd = reinterpret_cast<HWND>(widget->winId());
      const auto  ref = [](const QColor &c) -> COLORREF
      { return RGB(c.red(), c.green(), c.blue()); };

      const BOOL     dark = TRUE;
      const COLORREF caption = ref(colors.bg_deep);
      const COLORREF text = ref(colors.text_primary);
      const COLORREF edge = ref(mix(colors.bg_primary, colors.border, 0.38));

      // dark caption everywhere; exact colours on Windows 11 (ignored before)
      ::DwmSetWindowAttribute(hwnd,
                              20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */,
                              &dark,
                              sizeof(dark));
      ::DwmSetWindowAttribute(hwnd,
                              35 /* DWMWA_CAPTION_COLOR */,
                              &caption,
                              sizeof(caption));
      ::DwmSetWindowAttribute(hwnd, 36 /* DWMWA_TEXT_COLOR */, &text, sizeof(text));
      ::DwmSetWindowAttribute(hwnd, 34 /* DWMWA_BORDER_COLOR */, &edge, sizeof(edge));
    }
#endif
    return false;
  }

private:
  static void polish_message_box(QMessageBox *box)
  {
    if (box->property("_hsd_polished").toBool())
      return;
    box->setProperty("_hsd_polished", true);

    if (box->icon() != QMessageBox::NoIcon)
      box->setIconPixmap(MessageDialog::badge(kind_from_icon(box->icon()),
                                              40,
                                              box->devicePixelRatioF()));

    for (QAbstractButton *button : box->buttons())
    {
      const QMessageBox::ButtonRole role = box->buttonRole(button);

      QString kind = "secondary";
      if (role == QMessageBox::DestructiveRole)
        kind = "danger";
      else if (button == box->defaultButton())
        kind = "primary";

      button->setProperty("hsdRole", kind);
      button->style()->unpolish(button);
      button->style()->polish(button);
    }
  }
};

} // namespace

void install_native_dialog_polish(QApplication &app)
{
  app.installEventFilter(new NativeDialogPolish(&app));
}

} // namespace hesiod
