/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once

#include <algorithm>
#include <QPainter>
#include <QResizeEvent>
#include <QToolButton>

#include "meta_qt/ui/theme.hpp"

namespace hesiod
{

inline QString properties_action_style(const meta::qt::Theme &theme)
{
  return QString("QWidget#PropertiesPanelActions { background: transparent; border: none; }"
                 "QToolButton { background: transparent; border: 1px solid transparent;"
                 " border-radius: 6px; padding: 0px; }"
                 "QToolButton:hover, QToolButton:pressed { background: %1; }"
                 "QToolButton:checked { background: %1; border-color: %2; }"
                 "QToolButton:focus { border-color: %2; }"
                 "QToolButton::menu-indicator { image: none; width: 0px; }")
      .arg(theme.section_header_hover.name(), theme.accent.name());
}

/// A node's title, pin and existing actions share the same card alignment.
/// The title keeps its full accessible name and tooltip when visually elided.
class PropertiesPanelHeader : public QWidget
{
public:
  PropertiesPanelHeader(const QString &title, const meta::qt::Theme &theme,
                        QToolButton *pin, QWidget *actions,
                        QWidget *parent = nullptr)
      : QWidget(parent), title_(title), theme_(theme), pin_(pin), actions_(actions)
  {
    setAccessibleName(title);
    setToolTip(title);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    pin_->setParent(this);
    pin_->setFixedSize(28, 28);
    pin_->setIconSize(QSize(16, 16));
    pin_->setStyleSheet(properties_action_style(theme));
    if (actions_) actions_->setParent(this);
    setFixedHeight(56);
  }

  QSize sizeHint() const override { return QSize(420, height()); }
  QSize minimumSizeHint() const override { return QSize(220, height()); }

protected:
  void resizeEvent(QResizeEvent *event) override
  {
    const int inset = theme_.metrics.section_card_margin + 12;
    const int action_width = actions_ ? actions_->sizeHint().width() : 0;
    const int title_width = std::min(180, QFontMetrics(meta::qt::ui_font(13, true))
                                           .horizontalAdvance(title_));
    stacked_ = actions_ && width() < 2 * inset + 36 + title_width + 12 + action_width;
    setFixedHeight(stacked_ ? 88 : 56);
    pin_->setGeometry(inset, 14, 28, 28);
    if (actions_)
      actions_->setGeometry(stacked_ ? inset + 36 : width() - inset - action_width,
                            stacked_ ? 48 : 14, action_width, 28);
    QWidget::resizeEvent(event);
  }

  void paintEvent(QPaintEvent *) override
  {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const int margin = theme_.metrics.section_card_margin;
    const QRect card = rect().adjusted(margin, 5, -margin, -5);
    painter.setPen(Qt::NoPen);
    painter.setBrush(theme_.page.lighter(125));
    painter.drawRoundedRect(card, theme_.metrics.section_card_radius,
                            theme_.metrics.section_card_radius);
    painter.setFont(meta::qt::ui_font(13, true));
    painter.setPen(theme_.ink_primary);
    const int left = pin_->geometry().right() + 9;
    const int right = actions_ && !stacked_ ? actions_->x() - 12 : card.right() - 12;
    const QRect text(left, 14, std::max(0, right - left), 28);
    painter.drawText(text, Qt::AlignLeft | Qt::AlignVCenter,
                     painter.fontMetrics().elidedText(title_, Qt::ElideRight, text.width()));
  }

private:
  QString title_;
  const meta::qt::Theme &theme_;
  QToolButton *pin_;
  QWidget *actions_;
  bool stacked_ = false;
};

} // namespace hesiod
