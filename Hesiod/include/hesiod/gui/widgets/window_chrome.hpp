/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <functional>

#include <QAbstractButton>
#include <QPointer>
#include <QWidget>

class QHBoxLayout;
class QLabel;
class QMenuBar;

namespace hesiod
{

// =====================================
// WindowButton
// =====================================

// Minimize / maximize / close caption button. The glyph is painted rather than
// set as text: font glyphs such as U+00D7 sit on the text baseline, so they
// never land in the geometric centre of the button and drift with the font.
class WindowButton : public QAbstractButton
{
public:
  enum class Kind
  {
    Minimize,
    Maximize,
    Close
  };

  explicit WindowButton(Kind kind, QWidget *parent = nullptr);

  // draws the "restore" glyph (two stacked squares) instead of a single square
  void set_maximized(bool new_state);

  QSize sizeHint() const override;

protected:
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private:
  Kind kind;
  bool maximized = false;
  bool hovered = false;
};

// =====================================
// PanelFrame
// =====================================

// Rounded, bordered card that hosts one workspace pane (viewer, graph,
// settings...) over the darker application background, so each pane reads as
// its own surface. Children paint square corners, including the OpenGL viewer,
// so four small caps are stacked above the content to cut the corners round.
//
// The frame follows its content's visibility: hiding the content (e.g. the
// View menu hiding the viewer) hides the whole card instead of leaving an empty
// frame behind.
class PanelFrame : public QWidget
{
public:
  explicit PanelFrame(QWidget *content, QWidget *parent = nullptr);

  QWidget *content() const { return this->p_content; }

  static constexpr int radius = 8;

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  void place_corners();

  QPointer<QWidget> p_content;
  QWidget          *corners[4] = {nullptr, nullptr, nullptr, nullptr};
};

// =====================================
// TitleBar
// =====================================

class TitleChip;

// Two or more mutually exclusive options drawn as one pill (e.g. 2D | 3D).
class SegmentedControl : public QWidget
{
public:
  SegmentedControl(const QStringList &labels, QWidget *parent = nullptr);

  int  current() const { return this->index; }
  void set_current(int new_index); // does not call on_changed

  void set_tooltips(const QStringList &tips);

  std::function<void(int)> on_changed;

  QSize sizeHint() const override;

protected:
  void leaveEvent(QEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private:
  int segment_at(const QPoint &pos) const;

  QStringList labels, tips;
  int         index = 0;
  int         hovered = -1;
};

// Replaces the native caption: menu bar on the left, the project chip centred,
// optional controls and the caption buttons on the right. Dragging,
// double-click maximize and snapping are left to the OS (the window reports
// this bar as its caption area), so the bar itself only has to say which of
// its pixels are "empty".
//
// The project chip is interactive: a click asks for the project menu
// (on_title_clicked, with the point to open it at), a double-click asks for a
// rename (on_title_rename_requested), and begin_title_rename() edits the name
// in place.
class TitleBar : public QWidget
{
public:
  explicit TitleBar(QWidget *window);

  QMenuBar *menu_bar() const { return this->p_menu_bar; }

  // true when local_pos is bare title bar (not a menu entry, the chip or a
  // button), i.e. where a press should start a window move
  bool is_caption_area(const QPoint &local_pos) const;

  bool is_window_button(const QWidget *widget) const;

  // extra control shown just left of the caption buttons
  void add_trailing_widget(QWidget *widget);

  // `saved` false marks a project that has no file yet
  void set_project_title(const QString &name,
                         const QString &path,
                         bool           dirty,
                         bool           saved);
  void set_maximized(bool new_state);
  void set_window_buttons_visible(bool new_state);

  void begin_title_rename(const QString                       &current,
                          std::function<void(const QString &)> commit);

  std::function<void(const QPoint &global_pos)> on_title_clicked;
  std::function<void()>                         on_title_rename_requested;

protected:
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  void place_title();

  QWidget      *p_window;
  QMenuBar     *p_menu_bar;
  TitleChip    *p_chip;
  QHBoxLayout  *p_trailing;
  WindowButton *p_min;
  WindowButton *p_max;
  WindowButton *p_close;
};

} // namespace hesiod
