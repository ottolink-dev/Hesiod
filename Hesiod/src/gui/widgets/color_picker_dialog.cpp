/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#include <algorithm>
#include <functional>

#include <QAbstractButton>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

#include "meta_qt/ui/color_picker.hpp"
#include "meta_qt/ui/theme.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/color_picker_dialog.hpp"
#include "hesiod/gui/widgets/gui_utils.hpp"

namespace hesiod
{

namespace
{

// colours confirmed with OK this session, most recent first
QList<QColor> g_recent;

QColor edge_color()
{
  return panel_border_color(); // the card border, shared
}

// checkerboard under translucent colours
void paint_checker(QPainter &p, const QRectF &rect, qreal cell = 6.0)
{
  p.save();
  p.setClipRect(rect, Qt::IntersectClip);
  p.fillRect(rect, QColor("#9a9a9a"));
  for (qreal y = rect.top(); y < rect.bottom(); y += cell)
    for (qreal x = rect.left(); x < rect.right(); x += cell)
      if ((int((x - rect.left()) / cell) + int((y - rect.top()) / cell)) % 2 == 0)
        p.fillRect(QRectF(x, y, cell, cell), QColor("#666666"));
  p.restore();
}

// white ring with a dark outline: readable on any colour
void paint_handle_ring(QPainter &p, const QPointF &c, qreal r)
{
  p.setBrush(Qt::NoBrush);
  p.setPen(QPen(QColor(0, 0, 0, 140), 3.0));
  p.drawEllipse(c, r, r);
  p.setPen(QPen(QColor("#ffffff"), 1.8));
  p.drawEllipse(c, r, r);
}

// ---------------------------------------------------------------------------
// Swatch: one quick-pick colour
// ---------------------------------------------------------------------------

class Swatch final : public QAbstractButton
{
public:
  Swatch(const QColor &color, QWidget *parent) : QAbstractButton(parent), color(color)
  {
    this->setFixedSize(22, 22);
    this->setCursor(Qt::PointingHandCursor);
    this->setToolTip(
        color.name(color.alpha() < 255 ? QColor::HexArgb : QColor::HexRgb).toUpper());
  }

  QColor color;

protected:
  void paintEvent(QPaintEvent *) override
  {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(this->rect()).adjusted(1.5, 1.5, -1.5, -1.5);

    QPainterPath shape;
    shape.addRoundedRect(r, 5, 5);
    p.save();
    p.setClipPath(shape);
    if (this->color.alpha() < 255)
      paint_checker(p, r, 5.0);
    p.fillRect(r, this->color);
    p.restore();

    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(this->underMouse() ? QColor("#ffffff") : edge_color(), 1));
    p.drawPath(shape);
  }

  void enterEvent(QEnterEvent *event) override
  {
    this->update();
    QAbstractButton::enterEvent(event);
  }

  void leaveEvent(QEvent *event) override
  {
    this->update();
    QAbstractButton::leaveEvent(event);
  }
};

} // namespace

// ---------------------------------------------------------------------------
// ColorPlane: saturation (x) / value (y) for the current hue
// ---------------------------------------------------------------------------

class ColorPlane final : public QWidget
{
public:
  explicit ColorPlane(QWidget *parent) : QWidget(parent)
  {
    this->setFixedSize(240, 200);
    this->setCursor(Qt::CrossCursor);
  }

  void set(qreal h, qreal s, qreal v)
  {
    this->hue = h;
    this->sat = s;
    this->val = v;
    this->update();
  }

  std::function<void(qreal, qreal)> on_changed;

protected:
  void paintEvent(QPaintEvent *) override
  {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF r = QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath shape;
    shape.addRoundedRect(r, 7, 7);

    p.save();
    p.setClipPath(shape);
    p.fillRect(r, QColor::fromHsvF(this->hue, 1.0, 1.0));

    QLinearGradient white(r.topLeft(), r.topRight());
    white.setColorAt(0.0, QColor(255, 255, 255, 255));
    white.setColorAt(1.0, QColor(255, 255, 255, 0));
    p.fillRect(r, white);

    QLinearGradient black(r.topLeft(), r.bottomLeft());
    black.setColorAt(0.0, QColor(0, 0, 0, 0));
    black.setColorAt(1.0, QColor(0, 0, 0, 255));
    p.fillRect(r, black);
    p.restore();

    p.setPen(QPen(edge_color(), 1));
    p.setBrush(Qt::NoBrush);
    p.drawPath(shape);

    const QPointF c(r.left() + this->sat * r.width(),
                    r.top() + (1.0 - this->val) * r.height());
    paint_handle_ring(p, c, 6.5);
  }

  void mousePressEvent(QMouseEvent *event) override { this->pick(event->position()); }

  void mouseMoveEvent(QMouseEvent *event) override
  {
    if (event->buttons() & Qt::LeftButton)
      this->pick(event->position());
  }

private:
  void pick(const QPointF &pos)
  {
    this->sat = std::clamp(pos.x() / this->width(), 0.0, 1.0);
    this->val = std::clamp(1.0 - pos.y() / this->height(), 0.0, 1.0);
    this->update();
    if (this->on_changed)
      this->on_changed(this->sat, this->val);
  }

  qreal hue = 0.0, sat = 0.0, val = 0.0;
};

// ---------------------------------------------------------------------------
// ColorSlider: vertical hue or alpha strip
// ---------------------------------------------------------------------------

class ColorSlider final : public QWidget
{
public:
  enum class Mode
  {
    Hue,
    Alpha
  };

  ColorSlider(Mode mode, QWidget *parent) : QWidget(parent), mode(mode)
  {
    this->setFixedSize(22, 200);
    this->setCursor(Qt::PointingHandCursor);
    this->setToolTip(mode == Mode::Hue ? "Hue" : "Opacity");
  }

  void set_value(qreal v)
  {
    this->value = std::clamp(v, 0.0, 1.0);
    this->update();
  }

  void set_base(const QColor &color)
  {
    this->base = color;
    this->update();
  }

  std::function<void(qreal)> on_changed;

protected:
  void paintEvent(QPaintEvent *) override
  {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF track = this->track();
    QPainterPath shape;
    shape.addRoundedRect(track, 6, 6);

    p.save();
    p.setClipPath(shape);
    QLinearGradient gradient(track.topLeft(), track.bottomLeft());
    if (this->mode == Mode::Hue)
    {
      for (int i = 0; i <= 6; ++i)
        gradient.setColorAt(i / 6.0,
                            QColor::fromHsvF(std::min(i / 6.0, 0.9999), 1.0, 1.0));
    }
    else
    {
      paint_checker(p, track, 5.0);
      QColor opaque = this->base;
      opaque.setAlphaF(1.0);
      QColor clear = this->base;
      clear.setAlphaF(0.0);
      gradient.setColorAt(0.0, opaque);
      gradient.setColorAt(1.0, clear);
    }
    p.fillRect(track, gradient);
    p.restore();

    p.setPen(QPen(edge_color(), 1));
    p.setBrush(Qt::NoBrush);
    p.drawPath(shape);

    // handle: a pill across the strip
    const qreal  y = track.top() + this->position() * track.height();
    const QRectF pill(0.5, y - 4.0, this->width() - 1.0, 8.0);
    p.setPen(QPen(QColor(0, 0, 0, 140), 3.0));
    p.drawRoundedRect(pill, 4, 4);
    p.setPen(QPen(QColor("#ffffff"), 1.8));
    p.drawRoundedRect(pill, 4, 4);
  }

  void mousePressEvent(QMouseEvent *event) override { this->pick(event->position().y()); }

  void mouseMoveEvent(QMouseEvent *event) override
  {
    if (event->buttons() & Qt::LeftButton)
      this->pick(event->position().y());
  }

private:
  // inset so the handle stays whole at both ends
  QRectF track() const { return QRectF(this->rect()).adjusted(3.5, 5.5, -3.5, -5.5); }

  // hue runs top to bottom; alpha is opaque at the top
  qreal position() const
  {
    return this->mode == Mode::Hue ? this->value : 1.0 - this->value;
  }

  void pick(qreal y)
  {
    const QRectF r = this->track();
    const qreal  t = std::clamp((y - r.top()) / r.height(), 0.0, 1.0);
    this->value = this->mode == Mode::Hue ? std::min(t, 0.9999) : 1.0 - t;
    this->update();
    if (this->on_changed)
      this->on_changed(this->value);
  }

  Mode   mode;
  qreal  value = 0.0;
  QColor base = Qt::white;
};

// ---------------------------------------------------------------------------
// ColorPreview: new over current; clicking "current" goes back to it
// ---------------------------------------------------------------------------

class ColorPreview final : public QWidget
{
public:
  ColorPreview(const QColor &current, QWidget *parent) : QWidget(parent), current(current)
  {
    this->setFixedHeight(64);
    this->setToolTip("Top: new color. Bottom: current color (click to go back to it).");
  }

  void set_new(const QColor &color)
  {
    this->fresh = color;
    this->update();
  }

  std::function<void(const QColor &)> on_revert;

protected:
  void paintEvent(QPaintEvent *) override
  {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF r = QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath shape;
    shape.addRoundedRect(r, 7, 7);

    p.save();
    p.setClipPath(shape);
    paint_checker(p, r);
    const QRectF top(r.left(), r.top(), r.width(), r.height() / 2.0);
    const QRectF bottom(r.left(), top.bottom(), r.width(), r.height() / 2.0);
    p.fillRect(top, this->fresh);
    p.fillRect(bottom, this->current);

    // captions in whichever of black / white reads on the colour below
    const auto caption =
        [&p](const QRectF &area, const QColor &under, const QString &text)
    {
      const qreal luma = 0.299 * under.redF() + 0.587 * under.greenF() +
                         0.114 * under.blueF();
      QColor ink = (luma > 0.6 && under.alphaF() > 0.4) ? QColor(0, 0, 0, 170)
                                                        : QColor(255, 255, 255, 200);
      p.setPen(ink);
      p.setFont(meta::qt::ui_font(11));
      p.drawText(area.adjusted(9, 0, -9, 0), Qt::AlignVCenter | Qt::AlignLeft, text);
    };
    caption(top, this->fresh, "New");
    caption(bottom, this->current, "Current");
    p.restore();

    p.setPen(QPen(edge_color(), 1));
    p.setBrush(Qt::NoBrush);
    p.drawPath(shape);
  }

  void mousePressEvent(QMouseEvent *event) override
  {
    if (event->position().y() > this->height() / 2.0 && this->on_revert)
      this->on_revert(this->current);
  }

private:
  QColor current, fresh;
};

// =====================================
// ColorPickerDialog
// =====================================

ColorPickerDialog::ColorPickerDialog(const QColor  &initial,
                                     QWidget       *parent,
                                     const QString &title,
                                     bool           alpha)
    : MessageDialog(parent,
                    MessageDialog::Kind::None,
                    title.isEmpty() ? QString("Pick a color") : title,
                    QString()),
      with_alpha(alpha)
{
  const auto  &colors = HSD_CTX.app_settings.colors;
  const QColor start = initial.isValid() ? initial : QColor(Qt::white);

  this->setWindowTitle(title.isEmpty() ? QString("Pick a color") : title);
  this->set_card_width(alpha ? 600 : 560);

  // --- plane | hue | alpha | fields
  auto *top = new QHBoxLayout();
  top->setSpacing(10);

  this->plane = new ColorPlane(this);
  top->addWidget(this->plane, 0, Qt::AlignLeft | Qt::AlignTop);

  this->hue_slider = new ColorSlider(ColorSlider::Mode::Hue, this);
  top->addWidget(this->hue_slider, 0, Qt::AlignTop);

  if (alpha)
  {
    this->alpha_slider = new ColorSlider(ColorSlider::Mode::Alpha, this);
    top->addWidget(this->alpha_slider, 0, Qt::AlignTop);
  }

  top->addSpacing(6);

  auto *side = new QVBoxLayout();
  side->setSpacing(10);

  this->preview = new ColorPreview(start, this);
  side->addWidget(this->preview);

  auto *fields = new QGridLayout();
  fields->setHorizontalSpacing(6);
  fields->setVerticalSpacing(4);

  auto *hex_label = new QLabel("Hex", this);
  hex_label->setObjectName("fieldLabel");
  fields->addWidget(hex_label, 0, 0, 1, alpha ? 4 : 3);
  this->hex = new QLineEdit(this);
  this->hex->setObjectName("hexField");
  this->hex->setFont(meta::qt::mono_font(13));
  this->hex->setMaxLength(alpha ? 9 : 7);
  fields->addWidget(this->hex, 1, 0, 1, alpha ? 4 : 3);

  const char *names[] = {"R", "G", "B", "A"};
  const int   count = alpha ? 4 : 3;
  for (int i = 0; i < count; ++i)
  {
    auto *label = new QLabel(names[i], this);
    label->setObjectName("fieldLabel");
    label->setAlignment(Qt::AlignCenter);
    fields->addWidget(label, 2, i);

    auto *spin = new QSpinBox(this);
    spin->setRange(0, 255);
    spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    spin->setAlignment(Qt::AlignCenter);
    spin->setKeyboardTracking(false);
    spin->setObjectName("channelField");
    spin->setFixedWidth(46);
    this->channels[i] = spin;
    fields->addWidget(spin, 3, i);
  }
  side->addLayout(fields);
  side->addStretch(1);
  top->addLayout(side, 1);

  this->body()->addLayout(top);

  // --- swatches: theme colours, then recent picks
  auto add_swatch_row = [this](const QString &caption, const QList<QColor> &palette)
  {
    if (palette.isEmpty())
      return;

    auto *row = new QHBoxLayout();
    row->setSpacing(5);
    auto *label = new QLabel(caption, this);
    label->setObjectName("fieldLabel");
    label->setFixedWidth(52);
    row->addWidget(label);
    for (const QColor &color : palette)
    {
      auto *swatch = new Swatch(color, this);
      QObject::connect(swatch,
                       &QAbstractButton::clicked,
                       this,
                       [this, swatch]()
                       { this->set_from_color(swatch->color, Source::Swatch); });
      row->addWidget(swatch);
    }
    row->addStretch(1);
    this->body()->addLayout(row);
  };

  this->body()->addSpacing(4);
  add_swatch_row("Theme",
                 {colors.bg_deep,
                  colors.bg_primary,
                  colors.bg_secondary,
                  colors.border,
                  colors.text_secondary,
                  colors.text_primary,
                  colors.accent,
                  QColor("#4aaa6a"),
                  QColor("#d9a441"),
                  QColor("#e06c62")});
  add_swatch_row("Recent", g_recent);

  // --- buttons
  this->add_button("Cancel", MessageDialog::Role::Secondary, false, true);
  this->add_button("OK", MessageDialog::Role::Primary, true);

  // --- wiring
  this->plane->on_changed = [this](qreal s, qreal v)
  { this->set_hsv(this->hue, s, v, this->alpha_value, Source::Plane); };

  this->hue_slider->on_changed = [this](qreal h)
  { this->set_hsv(h, this->sat, this->val, this->alpha_value, Source::Hue); };

  if (this->alpha_slider)
    this->alpha_slider->on_changed = [this](qreal a)
    { this->set_hsv(this->hue, this->sat, this->val, a, Source::Alpha); };

  this->preview->on_revert = [this](const QColor &color)
  { this->set_from_color(color, Source::None); };

  QObject::connect(this->hex,
                   &QLineEdit::editingFinished,
                   this,
                   [this]()
                   {
                     QString text = this->hex->text().trimmed();
                     if (!text.startsWith('#'))
                       text.prepend('#');
                     const QColor parsed = QColor::fromString(text);
                     if (parsed.isValid())
                     {
                       QColor color = parsed;
                       // #RRGGBB typed into an alpha picker keeps the alpha
                       if (this->with_alpha && text.size() == 7)
                         color.setAlphaF(this->alpha_value);
                       this->set_from_color(color, Source::Hex);
                     }
                     else
                       this->set_hsv(this->hue,
                                     this->sat,
                                     this->val,
                                     this->alpha_value,
                                     Source::None); // restore the field
                   });

  for (int i = 0; i < count; ++i)
    QObject::connect(this->channels[i],
                     QOverload<int>::of(&QSpinBox::valueChanged),
                     this,
                     [this, count]()
                     {
                       QColor color(this->channels[0]->value(),
                                    this->channels[1]->value(),
                                    this->channels[2]->value(),
                                    count == 4 ? this->channels[3]->value() : 255);
                       if (count < 4)
                         color.setAlphaF(this->alpha_value);
                       this->set_from_color(color, Source::Channels);
                     });

  // --- extra styling
  QString                               css = R"(
    QDialog#hsdMessage QLabel#fieldLabel { color: FAINT; font-size: 11px; }
    QDialog#hsdMessage QLineEdit#hexField, QDialog#hsdMessage QSpinBox#channelField {
      background: DEEP; color: INK; border: 1px solid BORDER; border-radius: 6px;
      padding: 4px 6px; font-size: 12px; selection-background-color: ACCENT; }
    QDialog#hsdMessage QLineEdit#hexField:focus,
    QDialog#hsdMessage QSpinBox#channelField:focus { border-color: ACCENT; }
  )";
  const std::pair<const char *, QColor> tokens[] = {
      {"ACCENT", colors.accent},
      {"DEEP", colors.bg_deep},
      {"BORDER", edge_color()},
      {"FAINT", mix_colors(colors.bg_primary, colors.text_primary, 0.45)},
      {"INK", colors.text_primary}};
  for (const auto &[key, value] : tokens)
    css.replace(key, value.name());
  this->setStyleSheet(this->styleSheet() + css);

  this->set_from_color(start, Source::None);
}

QColor ColorPickerDialog::color() const
{
  return QColor::fromHsvF(this->hue, this->sat, this->val, this->alpha_value);
}

void ColorPickerDialog::set_from_color(const QColor &color, Source source)
{
  const QColor hsv = color.toHsv();

  // greys have no hue and black has no saturation: keep the previous ones so
  // the plane does not jump back to red while dragging through them
  const qreal h = hsv.hsvHueF() < 0.0 ? this->hue : hsv.hsvHueF();
  const qreal s = hsv.valueF() <= 0.0 ? this->sat : hsv.hsvSaturationF();

  this->set_hsv(h, s, hsv.valueF(), this->with_alpha ? color.alphaF() : 1.0, source);
}

void ColorPickerDialog::set_hsv(qreal h, qreal s, qreal v, qreal a, Source source)
{
  this->hue = std::clamp(h, 0.0, 0.9999);
  this->sat = std::clamp(s, 0.0, 1.0);
  this->val = std::clamp(v, 0.0, 1.0);
  this->alpha_value = std::clamp(a, 0.0, 1.0);

  const QColor color = this->color();

  this->plane->set(this->hue, this->sat, this->val);
  if (source != Source::Hue)
    this->hue_slider->set_value(this->hue);
  if (this->alpha_slider)
  {
    this->alpha_slider->set_base(color);
    if (source != Source::Alpha)
      this->alpha_slider->set_value(this->alpha_value);
  }

  this->preview->set_new(color);

  if (source != Source::Hex)
  {
    const QSignalBlocker block(this->hex);
    this->hex->setText(
        color.name(this->with_alpha ? QColor::HexArgb : QColor::HexRgb).toUpper());
  }

  if (source != Source::Channels)
  {
    const int values[] = {color.red(), color.green(), color.blue(), color.alpha()};
    for (int i = 0; i < 4; ++i)
      if (this->channels[i])
      {
        const QSignalBlocker block(this->channels[i]);
        this->channels[i]->setValue(values[i]);
      }
  }
}

QColor ColorPickerDialog::get_color(const QColor  &initial,
                                    QWidget       *parent,
                                    const QString &title,
                                    bool           alpha)
{
  ColorPickerDialog dialog(initial, parent, title, alpha);
  if (dialog.exec() != QDialog::Accepted)
    return QColor();

  const QColor picked = dialog.color();

  g_recent.removeAll(picked);
  g_recent.prepend(picked);
  while (g_recent.size() > 10)
    g_recent.removeLast();

  return picked;
}

void install_color_picker()
{
  meta::qt::set_color_picker(
      [](const QColor &initial, QWidget *parent, const QString &title, bool alpha)
      { return ColorPickerDialog::get_color(initial, parent, title, alpha); });
}

} // namespace hesiod
