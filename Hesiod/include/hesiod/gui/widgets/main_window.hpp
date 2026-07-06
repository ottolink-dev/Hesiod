/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QCloseEvent>
#include <QMainWindow>
#include <QProgressBar>

namespace hesiod
{

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  MainWindow(QWidget *parent = nullptr);

  void notify(const std::string &msg = "", int timeout = 5000);

  void restore_geometry();
  void save_geometry() const;
  void setup_connections_with_project();

protected:
  // settings are otherwise only saved through File > Quit; make sure they also
  // persist when the window is closed by the window manager
  void closeEvent(QCloseEvent *event) override;

private:
  void setup_progress_bar();

  QProgressBar *progress_bar;
};

} // namespace hesiod