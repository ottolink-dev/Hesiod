/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once
#include <QString>
#include <QWidget>

namespace meta::qt
{
struct Theme;
}

namespace hesiod
{

/// A stage with nothing to set (invert) still gets a line saying it is there.
class PostStageLabel final : public QWidget
{
public:
  PostStageLabel(const QString &text, const meta::qt::Theme &theme, QWidget *parent);

protected:
  void paintEvent(QPaintEvent *) override;

private:
  QString                text;
  const meta::qt::Theme &theme;
};

} // namespace hesiod
