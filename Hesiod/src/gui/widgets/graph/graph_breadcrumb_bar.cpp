/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#include "hesiod/gui/widgets/graph/graph_breadcrumb_bar.hpp"

#include <QLabel>

namespace hesiod
{

GraphBreadcrumbBar::GraphBreadcrumbBar(QWidget *parent) : QWidget(parent)
{
  this->setObjectName("hsdBreadcrumbBar");
  this->setFixedHeight(28);

  this->layout = new QHBoxLayout(this);
  this->layout->setContentsMargins(8, 2, 8, 2);
  this->layout->setSpacing(4);
  this->setLayout(this->layout);

  this->setStyleSheet(
      "QWidget#hsdBreadcrumbBar { background-color: #1e1e1e; border-bottom: 1px solid #333333; }");
}

void GraphBreadcrumbBar::set_path(
    const std::vector<std::pair<std::string, std::string>> &path)
{
  // Clear existing items
  QLayoutItem *child = nullptr;
  while ((child = this->layout->takeAt(0)) != nullptr)
  {
    if (child->widget())
      child->widget()->deleteLater();
    delete child;
  }

  if (path.empty())
    return;

  for (size_t i = 0; i < path.size(); ++i)
  {
    const auto &[id, label] = path[i];

    auto *btn = new QPushButton(QString::fromStdString(label.empty() ? id : label), this);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFlat(true);

    if (i == path.size() - 1)
    {
      // Current active node (last in path)
      btn->setStyleSheet(
          "QPushButton { color: #ffffff; font-weight: bold; font-size: 11px; padding: 2px 6px; "
          "background: transparent; border: none; }");
    }
    else
    {
      // Ancestor nodes
      btn->setStyleSheet(
          "QPushButton { color: #88aaff; font-size: 11px; padding: 2px 6px; background: transparent; "
          "border: none; } "
          "QPushButton:hover { color: #bcd4ff; text-decoration: underline; }");

      this->connect(btn,
                    &QPushButton::clicked,
                    this,
                    [this, i, id]() { emit item_clicked(i, id); });
    }

    this->layout->addWidget(btn);

    // Separator between items
    if (i + 1 < path.size())
    {
      auto *sep = new QLabel(">", this);
      sep->setStyleSheet("QLabel { color: #666666; font-size: 10px; }");
      this->layout->addWidget(sep);
    }
  }

  this->layout->addStretch();
}

} // namespace hesiod
