/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QCheckBox>
#include <QIcon>
#include <QWidget>

namespace hesiod
{

class IconCheckBox : public QCheckBox
{
  Q_OBJECT

public:
  explicit IconCheckBox(QWidget *parent = nullptr);

  void set_icons(const QIcon &unchecked_icon, const QIcon &checked_icon);
  void set_icon_size(int new_size);
  void set_label(const QString &text);

protected:
  void  paintEvent(QPaintEvent *event) override;
  QSize sizeHint() const override;

private:
  QIcon   unchecked_icon;
  QIcon   checked_icon;
  QString label_text;
  int     icon_size = 24;
  int     spacing = 6; // space between icon and label
};

} // namespace hesiod