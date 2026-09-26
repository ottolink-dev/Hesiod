/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QCloseEvent>
#include <QMainWindow>
#include <QMenuBar>
#include <QProgressBar>

namespace hesiod
{

class TitleBar;

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  MainWindow(QWidget *parent = nullptr);

  // the menu bar lives inside the custom title bar; use this rather than
  // QMainWindow::menuBar(), which would replace the title bar with a new bar
  QMenuBar *menu_bar() const;
  TitleBar *get_title_bar() const { return this->title_bar; }

  void notify(const std::string &msg = "", int timeout = 5000);

  void restore_geometry();
  void save_geometry() const;
  void set_project_title(const std::string &name, const std::string &path, bool dirty);
  void setup_connections_with_project();

protected:
  void changeEvent(QEvent *event) override;

  // settings are otherwise only saved through File > Quit; make sure they also
  // persist when the window is closed by the window manager
  void closeEvent(QCloseEvent *event) override;

  bool nativeEvent(const QByteArray &event_type, void *message, qintptr *result) override;

private:
  void setup_frameless_window();
  void setup_progress_bar();
  void setup_status_bar();

  TitleBar     *title_bar = nullptr;
  QProgressBar *progress_bar;
  bool          frameless = false;
};

} // namespace hesiod
