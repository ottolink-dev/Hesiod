/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>

#include <QEnterEvent>
#include <QFontMetricsF>
#include <QImageReader>
#include <QKeyEvent>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include "hesiod/gui/widgets/controls/control_tones.hpp"
#include "hesiod/gui/widgets/dialogs/start_screen/example_tile.hpp"
#include "hesiod/gui/widgets/dialogs/start_screen/start_screen_style.hpp"
#include "hesiod/gui/widgets/utils/gui_utils.hpp"
#include "meta_qt/ui/theme.hpp"

namespace hesiod
{

namespace ss = start_screen;

namespace
{

// A picture no wider than it will ever be shown, so eight previews do not hold
// eight full-size photographs in memory.
QImage load_preview(const QString &path, int max_width)
{
  if (path.isEmpty())
    return {};

  QImageReader reader(path);
  reader.setAutoTransform(true);
  const QSize size = reader.size();
  if (size.isValid() && size.width() > max_width)
    reader.setScaledSize(size.scaled(max_width, max_width, Qt::KeepAspectRatio));
  return reader.read();
}

// Fills the target with the middle of the picture, cropped rather than
// squeezed. A zoom above 1 crops a little further in.
void draw_cover(QPainter &p, const QPixmap &image, const QRectF &target, qreal zoom)
{
  if (image.isNull() || target.isEmpty())
    return;

  const qreal scale = std::max(target.width() / image.width(),
                               target.height() / image.height()) *
                      std::max<qreal>(1.0, zoom);
  const QSizeF source_size(target.width() / scale, target.height() / scale);
  const QRectF source((image.width() - source_size.width()) / 2.0,
                      (image.height() - source_size.height()) / 2.0,
                      source_size.width(),
                      source_size.height());
  p.setRenderHint(QPainter::SmoothPixmapTransform);
  p.drawPixmap(target, image, source);
}

} // namespace

ExampleTile::ExampleTile(const QString &title,
                         const QString &preview_path,
                         bool           hero,
                         QWidget       *parent)
    : QAbstractButton(parent), arrival(this), title(title), hero(hero)
{
  this->setCursor(Qt::PointingHandCursor);
  this->setFocusPolicy(Qt::StrongFocus);
  this->setAttribute(Qt::WA_Hover);
  this->setAccessibleName(title);
  this->source = load_preview(preview_path, ss::tile_preview_width);
}

void ExampleTile::enterEvent(QEnterEvent *event)
{
  // the picture takes its time; the rest answers at once
  this->zoom_t.to(1.0, motion::start_tile_zoom_in, motion::enter());
  this->hover_t.to(1.0, motion::hover_in, motion::enter());
  QAbstractButton::enterEvent(event);
}

void ExampleTile::leaveEvent(QEvent *event)
{
  this->zoom_t.to(0.0, motion::start_tile_zoom_out, motion::change());
  this->hover_t.to(0.0, motion::hover_out, motion::change());
  QAbstractButton::leaveEvent(event);
}

void ExampleTile::mousePressEvent(QMouseEvent *event)
{
  if (event->button() == Qt::LeftButton)
    this->press_t.to(1.0, motion::press_in, motion::enter());
  QAbstractButton::mousePressEvent(event);
}

void ExampleTile::mouseReleaseEvent(QMouseEvent *event)
{
  this->press_t.to(0.0, motion::press_out, motion::change());
  QAbstractButton::mouseReleaseEvent(event);
}

void ExampleTile::keyPressEvent(QKeyEvent *event)
{
  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
  {
    this->click();
    return;
  }
  QAbstractButton::keyPressEvent(event);
}

// no picture: a calm surface with a ridge line, so the tile still reads as
// terrain
void ExampleTile::draw_placeholder(QPainter &p, const QRectF &box) const
{
  const ControlTones t = control_tones();
  p.fillRect(box, mix_colors(t.card, t.ink, 0.06));

  QPainterPath ridge;
  const qreal  b = box.bottom() - box.height() * 0.30;
  ridge.moveTo(box.left() + box.width() * 0.16, b);
  ridge.lineTo(box.left() + box.width() * 0.40, b - box.height() * 0.26);
  ridge.lineTo(box.left() + box.width() * 0.52, b - box.height() * 0.12);
  ridge.lineTo(box.left() + box.width() * 0.66, b - box.height() * 0.34);
  ridge.lineTo(box.left() + box.width() * 0.86, b);
  p.setPen(QPen(mix_colors(t.card, t.ink, 0.22),
                1.6,
                Qt::SolidLine,
                Qt::RoundCap,
                Qt::RoundJoin));
  p.setBrush(Qt::NoBrush);
  p.drawPath(ridge);
}

void ExampleTile::paintEvent(QPaintEvent *)
{
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  if (!this->arrival.apply(p, ss::tile_rise))
    return;

  const qreal hover = std::clamp(
      std::max(this->hover_t.value(), this->hasFocus() ? ss::focus_heat : 0.0),
      0.0,
      1.0);
  const qreal  zoom = std::clamp(this->zoom_t.value(), 0.0, 1.0);
  const qreal  held = std::clamp(this->press_t.value(), 0.0, 1.0);
  const QRectF box = QRectF(this->rect());

  QPainterPath shape;
  shape.addRoundedRect(box, ss::tile_radius, ss::tile_radius);
  p.setClipPath(shape);

  // --- the picture, scaled once to the size it is shown at (with room for
  // the zoom)
  const qreal dpr = this->devicePixelRatioF();
  const QSize want = (box.size() * dpr * (1.0 + ss::tile_zoom)).toSize();
  if (!this->source.isNull() && this->scaled_for != want)
  {
    this->scaled = QPixmap::fromImage(this->source.scaled(want,
                                                          Qt::KeepAspectRatioByExpanding,
                                                          Qt::SmoothTransformation));
    this->scaled_for = want;
  }

  if (!this->scaled.isNull())
    draw_cover(p, this->scaled, box, 1.0 + ss::tile_zoom * zoom);
  else
    this->draw_placeholder(p, box);

  // --- a shade along the bottom for the name to sit on; black and white
  // only, so the pictures keep their own colors
  const qreal     shade_h = std::min(box.height() * 0.62,
                                 this->hero ? ss::tile_shade_hero : ss::tile_shade);
  QLinearGradient shade(0.0, box.bottom() - shade_h, 0.0, box.bottom());
  shade.setColorAt(0.0, QColor(0, 0, 0, 0));
  shade.setColorAt(0.55, QColor(0, 0, 0, int(70 + 30 * hover)));
  shade.setColorAt(1.0, QColor(0, 0, 0, int(165 + 45 * hover)));
  p.fillRect(QRectF(box.left(), box.bottom() - shade_h, box.width(), shade_h), shade);

  // held down, the whole picture dims a touch
  if (held > 0.002)
    p.fillRect(box, QColor(0, 0, 0, int(46 * held)));

  // --- name
  const qreal inset = this->hero ? ss::tile_inset_hero : ss::tile_inset;
  const QFont title_font = meta::qt::ui_font(this->hero ? 18 : 13, true);
  p.setFont(title_font);
  p.setPen(QColor(255, 255, 255, int(232 + 23 * hover)));
  const qreal         title_room = box.width() - 2.0 * inset - 34.0;
  const QFontMetricsF fm(title_font);
  p.drawText(QRectF(box.left() + inset,
                    box.bottom() - inset - fm.height() + 2.0,
                    title_room,
                    fm.height()),
             Qt::AlignLeft | Qt::AlignVCenter,
             fm.elidedText(this->title, Qt::ElideRight, title_room));

  if (this->hero)
  {
    p.setFont(meta::qt::ui_font(11, true, 0.6));
    p.setPen(QColor(255, 255, 255, 170));
    p.drawText(QRectF(box.left() + inset,
                      box.bottom() - inset - fm.height() - 16.0,
                      title_room,
                      14.0),
               Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("FEATURED EXAMPLE"));
  }

  // --- where a click goes: a round arrow that comes up as the pointer arrives
  if (hover > 0.01)
  {
    const qreal   r = this->hero ? ss::tile_arrow_hero : ss::tile_arrow;
    const QPointF c(box.right() - inset - r,
                    box.bottom() - inset - r + 5.0 * (1.0 - hover));
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255, int(236 * hover)));
    p.drawEllipse(c, r, r);

    p.setPen(QPen(QColor(24, 24, 24, int(255 * hover)),
                  1.6,
                  Qt::SolidLine,
                  Qt::RoundCap,
                  Qt::RoundJoin));
    const qreal a = r * 0.36;
    p.drawLine(c + QPointF(-a, 0.0), c + QPointF(a, 0.0));
    p.drawLine(c + QPointF(a, 0.0), c + QPointF(a * 0.15, -a * 0.85));
    p.drawLine(c + QPointF(a, 0.0), c + QPointF(a * 0.15, a * 0.85));
  }

  // --- a hairline of light on the edge, which is what separates a dark
  // picture from the dark page
  p.setClipping(false);
  p.setBrush(Qt::NoBrush);
  p.setPen(QPen(QColor(255, 255, 255, int(18 + 34 * hover)), 1.0));
  p.drawRoundedRect(box.adjusted(0.5, 0.5, -0.5, -0.5), ss::tile_radius, ss::tile_radius);
}

} // namespace hesiod
