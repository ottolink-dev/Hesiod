/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <string>
#include <vector>

#include <QHBoxLayout>
#include <QPushButton>
#include <QWidget>

namespace hesiod
{

// =====================================
// GraphBreadcrumbBar
// =====================================
class GraphBreadcrumbBar : public QWidget
{
  Q_OBJECT

public:
  explicit GraphBreadcrumbBar(QWidget *parent = nullptr);

  void set_path(const std::vector<std::pair<std::string, std::string>> &path);

signals:
  void item_clicked(size_t index, const std::string &id);

private:
  QHBoxLayout *layout = nullptr;
};

} // namespace hesiod
