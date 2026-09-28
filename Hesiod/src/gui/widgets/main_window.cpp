/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>

#include <QGuiApplication>
#include <QMessageBox>
#include <QScreen>
#include <QStatusBar>

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/main_window.hpp"
#include "hesiod/gui/widgets/window_chrome.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/graph/graph_manager.hpp"

// last: windows.h leaks macros (interface, min, max...) that clash with the
// project headers above
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <dwmapi.h>
#include <windowsx.h>
#undef interface
#endif

namespace hesiod
{

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
  Logger::log()->trace("MainWindow::MainWindow");

  this->setObjectName("hsdMainWindow");

  this->title_bar = new TitleBar(this);
  this->setMenuWidget(this->title_bar);

  this->setup_frameless_window();
  this->title_bar->set_window_buttons_visible(this->frameless);

  this->restore_geometry();
  this->setup_status_bar();
  this->setup_progress_bar();
}

void MainWindow::changeEvent(QEvent *event)
{
  if (event->type() == QEvent::WindowStateChange && this->title_bar)
    this->title_bar->set_maximized(this->isMaximized());

  QMainWindow::changeEvent(event);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
  Logger::log()->trace("MainWindow::closeEvent");

  if (!HSD_APP->confirm_discard_unsaved_changes("Quit"))
  {
    event->ignore();
    return;
  }

  // a clean exit: whatever is on disk is either saved or explicitly discarded
  if (AutosaveManager *autosave = HSD_APP->get_autosave_manager_ref())
    autosave->discard();

  this->save_geometry();
  HSD_CTX.save_settings();

  QMainWindow::closeEvent(event);
}

QMenuBar *MainWindow::menu_bar() const { return this->title_bar->menu_bar(); }

bool MainWindow::nativeEvent(const QByteArray &event_type, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
  if (this->frameless && event_type == "windows_generic_MSG")
  {
    MSG *msg = static_cast<MSG *>(message);

    switch (msg->message)
    {
    case WM_NCCALCSIZE:
    {
      // the whole window is client area: no native caption, no visible frame.
      // The thick frame style stays on the window so that resizing, Aero snap
      // and the min/max animations keep working.
      if (msg->wParam == TRUE)
      {
        if (::IsZoomed(msg->hwnd))
        {
          // a maximized thick-frame window overhangs its monitor by the frame
          // width on every side; clamp the client to the work area instead
          auto       *params = reinterpret_cast<NCCALCSIZE_PARAMS *>(msg->lParam);
          HMONITOR    monitor = ::MonitorFromWindow(msg->hwnd, MONITOR_DEFAULTTONEAREST);
          MONITORINFO info{};
          info.cbSize = sizeof(info);
          if (::GetMonitorInfoW(monitor, &info))
            params->rgrc[0] = info.rcWork;
        }
        *result = 0;
        return true;
      }
      break;
    }

    case WM_GETMINMAXINFO:
    {
      // maximize onto the monitor's work area, not the whole monitor: the
      // window rectangle must stop at the taskbar, not merely its client area
      auto       *info = reinterpret_cast<MINMAXINFO *>(msg->lParam);
      HMONITOR    monitor = ::MonitorFromWindow(msg->hwnd, MONITOR_DEFAULTTONEAREST);
      MONITORINFO monitor_info{};
      monitor_info.cbSize = sizeof(monitor_info);
      if (!::GetMonitorInfoW(monitor, &monitor_info))
        break;

      const RECT &work = monitor_info.rcWork;
      const RECT &full = monitor_info.rcMonitor;
      info->ptMaxPosition.x = work.left - full.left;
      info->ptMaxPosition.y = work.top - full.top;
      info->ptMaxSize.x = work.right - work.left;
      info->ptMaxSize.y = work.bottom - work.top;

      // Qt normally fills in the minimum tracking size; it is skipped here
      const qreal dpr = this->devicePixelRatioF();
      const QSize min_size = this->minimumSize().expandedTo(QSize(480, 320));
      info->ptMinTrackSize.x = qRound(min_size.width() * dpr);
      info->ptMinTrackSize.y = qRound(min_size.height() * dpr);

      *result = 0;
      return true;
    }

    case WM_NCHITTEST:
    {
      RECT window_rect;
      ::GetWindowRect(msg->hwnd, &window_rect);

      const LONG x = GET_X_LPARAM(msg->lParam);
      const LONG y = GET_Y_LPARAM(msg->lParam);

      // resize band, in physical pixels
      const UINT dpi = ::GetDpiForWindow(msg->hwnd);
      const int  band = ::GetSystemMetricsForDpi(SM_CXSIZEFRAME, dpi) +
                       ::GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);

      if (!::IsZoomed(msg->hwnd))
      {
        const bool left = x < window_rect.left + band;
        const bool right = x >= window_rect.right - band;
        const bool top = y < window_rect.top + band;
        const bool bottom = y >= window_rect.bottom - band;

        if (top && left)
          *result = HTTOPLEFT;
        else if (top && right)
          *result = HTTOPRIGHT;
        else if (bottom && left)
          *result = HTBOTTOMLEFT;
        else if (bottom && right)
          *result = HTBOTTOMRIGHT;
        else if (left)
          *result = HTLEFT;
        else if (right)
          *result = HTRIGHT;
        else if (top)
          *result = HTTOP;
        else if (bottom)
          *result = HTBOTTOM;
        else
          *result = 0;

        if (*result)
          return true;
      }

      // physical -> logical window coordinates
      const qreal  dpr = this->devicePixelRatioF();
      const QPoint local(qFloor((x - window_rect.left) / dpr),
                         qFloor((y - window_rect.top) / dpr));

      if (this->title_bar && this->title_bar->geometry().contains(local))
      {
        const QPoint bar_pos = this->title_bar->mapFrom(this, local);
        if (this->title_bar->is_caption_area(bar_pos))
        {
          *result = HTCAPTION;
          return true;
        }
      }

      *result = HTCLIENT;
      return true;
    }

    default:
      break;
    }
  }
#else
  Q_UNUSED(event_type);
  Q_UNUSED(message);
  Q_UNUSED(result);
#endif

  return QMainWindow::nativeEvent(event_type, message, result);
}

void MainWindow::notify(const std::string &msg, int timeout)
{
  this->statusBar()->showMessage(msg.c_str(), timeout);
}

void MainWindow::set_project_title(const std::string &name,
                                   const std::string &path,
                                   bool               dirty)
{
  const bool    saved = !path.empty();
  const QString shown = name.empty() ? QString("Untitled") : QString::fromStdString(name);

  // the native title still matters: it is what the taskbar and Alt+Tab show
  QString title = saved ? QString("%1 [%2]").arg(shown, QString::fromStdString(path))
                        : QString("%1 (not saved)").arg(shown);
  if (dirty)
    title += "*";
  this->setWindowTitle(title);

  this->title_bar->set_project_title(shown, QString::fromStdString(path), dirty, saved);
}

void MainWindow::setup_frameless_window()
{
#ifdef Q_OS_WIN
  // Frameless for Qt (no frame margins in its geometry maths), but the native
  // window keeps the full overlapped style: WM_NCCALCSIZE above hides the
  // frame, while the OS keeps providing resize borders, snapping, the system
  // menu and the drop shadow.
  this->setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint |
                       Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);

  HWND hwnd = reinterpret_cast<HWND>(this->winId());
  if (!hwnd)
    return;

  LONG_PTR style = ::GetWindowLongPtrW(hwnd, GWL_STYLE);
  style &= ~static_cast<LONG_PTR>(WS_POPUP);
  style |= WS_OVERLAPPEDWINDOW;
  ::SetWindowLongPtrW(hwnd, GWL_STYLE, style);

  // a one pixel sliver of DWM frame is enough to get the native shadow back
  const MARGINS shadow = {1, 1, 1, 1};
  ::DwmExtendFrameIntoClientArea(hwnd, &shadow);

  // rounded corners on Windows 11 (ignored on older systems)
  const DWORD corner_preference = 2; // DWMWCP_ROUND
  ::DwmSetWindowAttribute(hwnd,
                          33 /* DWMWA_WINDOW_CORNER_PREFERENCE */,
                          &corner_preference,
                          sizeof(corner_preference));

  // dark caption colour for the few places Windows still draws one (snap
  // previews, the border on Windows 10)
  const BOOL dark = TRUE;
  ::DwmSetWindowAttribute(hwnd,
                          20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */,
                          &dark,
                          sizeof(dark));

  ::SetWindowPos(hwnd,
                 nullptr,
                 0,
                 0,
                 0,
                 0,
                 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                     SWP_NOOWNERZORDER | SWP_NOACTIVATE);

  this->frameless = true;
#endif
}

void MainWindow::setup_status_bar()
{
  // resizing is done from the window border; the grip only adds clutter
  this->statusBar()->setSizeGripEnabled(!this->frameless);
  this->statusBar()->setObjectName("hsdStatusBar");
}

void MainWindow::restore_geometry()
{
  Logger::log()->trace("MainWindow::restore_geometry");

  AppContext &ctx = HSD_CTX;

  QRect geom(ctx.app_settings.window.geom_main.x,
             ctx.app_settings.window.geom_main.y,
             ctx.app_settings.window.geom_main.w,
             ctx.app_settings.window.geom_main.h);

  // Saved geometry is in logical pixels, and raising the interface scale
  // shrinks the screen measured in those: a window saved full-screen at 100%
  // is larger than the whole desktop at 200%, with its title bar off the top
  // and no way to drag it back. Clamp to whatever screen it lands on.
  const QScreen *screen = QGuiApplication::screenAt(geom.center());
  if (!screen)
    screen = QGuiApplication::primaryScreen();

  if (screen)
  {
    const QRect available = screen->availableGeometry();

    // A geometry that is not usable at all is a stale one, not a preference:
    // fall back to a comfortable fraction of the screen rather than restoring a
    // sliver. Guards a config written before the window ever had a real size.
    if (geom.width() < 320 || geom.height() < 240)
      geom.setSize(QSize(available.width() * 3 / 4, available.height() * 3 / 4));

    geom.setSize(geom.size().boundedTo(available.size()));
    geom.moveLeft(
        std::clamp(geom.left(), available.left(), available.right() - geom.width() + 1));
    geom.moveTop(
        std::clamp(geom.top(), available.top(), available.bottom() - geom.height() + 1));
  }

  this->setGeometry(geom);
}

void MainWindow::save_geometry() const
{
  Logger::log()->trace("MainWindow::save_geometry");

  AppContext &ctx = HSD_CTX;

  QRect geom = this->geometry();
  ctx.app_settings.window.geom_main.x = geom.x();
  ctx.app_settings.window.geom_main.y = geom.y();
  ctx.app_settings.window.geom_main.w = geom.width();
  ctx.app_settings.window.geom_main.h = geom.height();
}

void MainWindow::setup_connections_with_project()
{
  Logger::log()->trace("MainWindow::setup_connections_with_project");

  AppContext &ctx = HSD_CTX;

  // make sure project is ready
  if (!ctx.project_model->get_graph_manager_ref())
  {
    Logger::log()->error("MainWindow::setup_connections_with_project: graph_manager "
                         "model ref is dangling ptr");
    return;
  }

  // GraphNode model -> MainWindow
  ctx.project_model->get_graph_manager_ref()->update_progress = [this](float progress)
  {
    QMetaObject::invokeMethod(
        this,
        [this, progress]()
        {
          if (progress == 0.f || progress == 100.f)
          {
            this->progress_bar->setValue(0);
            this->progress_bar->setTextVisible(false);

            const std::string message = (progress == 0.f) ? "Updating graph..."
                                                          : "Graph updated successfully.";

            this->notify(message);
            return;
          }

          this->progress_bar->setTextVisible(true);
          this->progress_bar->setValue(static_cast<int>(progress));
        },
        Qt::QueuedConnection);
  };

  ctx.project_model->get_graph_manager_ref()->update_failed =
      [this](const std::string &message)
  {
    QMetaObject::invokeMethod(
        this,
        [this, message]()
        {
          this->progress_bar->setValue(0);
          this->progress_bar->setTextVisible(false);

          this->notify(message);

          QMessageBox::warning(this,
                               tr("Graph update failed"),
                               QString::fromStdString(message));
        },
        Qt::QueuedConnection);
  };
}

void MainWindow::setup_progress_bar()
{
  Logger::log()->trace("MainWindow::setup_progress_bar");

  AppContext &ctx = HSD_CTX;

  this->progress_bar = new QProgressBar(this);
  this->progress_bar->setRange(0, 100);
  this->progress_bar->setValue(0);
  this->progress_bar->setTextVisible(false);
  this->progress_bar->setFixedWidth(ctx.app_settings.window.progress_bar_width);

  const std::string sheet = std::format(
      R"(
        QProgressBar {{
            border: 0px;
            border-radius: 3px;
            background-color: {};
            min-height: 6px;
            max-height: 6px;
            padding: 0px;
            margin-right: 6px;
            font-size: 9px;
            color: transparent;
        }}
        QProgressBar::chunk {{
            background-color: {};
            border-radius: 3px;
            margin: 0px;
        }}
    )",
      ctx.app_settings.colors.bg_primary.name().toStdString(),
      ctx.app_settings.colors.accent.name().toStdString());

  this->progress_bar->setStyleSheet(sheet.c_str());

  this->statusBar()->addPermanentWidget(this->progress_bar, 0);
}

} // namespace hesiod
