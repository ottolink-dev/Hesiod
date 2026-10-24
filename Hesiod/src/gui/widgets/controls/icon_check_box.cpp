/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QMouseEvent>
#include <QPainter>

#include "hesiod/gui/widgets/controls/icon_check_box.hpp"

namespace hesiod
{

IconCheckBox::IconCheckBox(QWidget *parent) : QCheckBox(parent)
{
  this->setCursor(Qt::PointingHandCursor);
}

void IconCheckBox::paintEvent(QPaintEvent *)
{
  QPainter painter(this);

  // Draw icon
  bool         checked = (this->checkState() == Qt::Checked);
  const QIcon &icon = checked ? this->checked_icon : this->unchecked_icon;
  QPixmap      pix = icon.pixmap(this->icon_size, this->icon_size);

  int y = (this->height() - this->icon_size) / 2;
  painter.drawPixmap(0, y, pix);

  // Draw label
  if (!this->label_text.isEmpty())
  {
    painter.setPen(this->palette().color(QPalette::WindowText));

    int text_x = this->icon_size + this->spacing;
    int text_y = (this->height() + painter.fontMetrics().ascent()) / 2 - 1;

    painter.drawText(text_x, text_y, this->label_text);
  }
}

void IconCheckBox::set_icons(const QIcon &unchecked_icon, const QIcon &checked_icon)
{
  this->unchecked_icon = unchecked_icon;
  this->checked_icon = checked_icon;
  this->update();
}

void IconCheckBox::set_icon_size(int new_size)
{
  this->icon_size = new_size;
  this->update();
}

void IconCheckBox::set_label(const QString &text)
{
  this->label_text = text;
  this->updateGeometry();
  this->update();
}

QSize IconCheckBox::sizeHint() const
{
  int width = this->icon_size;
  int height = this->icon_size;

  if (!this->label_text.isEmpty())
  {
    QFontMetrics fm(this->font());
    width += this->spacing + fm.horizontalAdvance(this->label_text);
    height = qMax(height, fm.height());
  }

  return QSize(width, height);
}

} // namespace hesiod
