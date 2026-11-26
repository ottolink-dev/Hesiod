/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once
#include <functional>

#include <QFont>
#include <QString>
#include <QTimer>
#include <QWidget>

#include "hesiod/gui/motion/motion.hpp"

class QLineEdit;

namespace hesiod
{

// The project in the title bar: its name, whether it has a file and unsaved
// changes, and a chevron for the project menu. A click opens that menu
// (on_clicked, with the point to open it at), and begin_rename() edits the
// name in place.
//
// The chip is as wide as what it shows. Every length in it is an animated
// number, so when the name changes, the "Not saved" note goes or the dot for
// unsaved changes arrives, the chip glides to its new width and the rest moves
// along instead of jumping. It does not place itself: it says how wide it
// wants to be (natural_width) and calls on_layout on every step that changes
// that, and the title bar centres it.
class TitleChip final : public QWidget
{
public:
  static constexpr int kHeight = 28;

  explicit TitleChip(QWidget *parent = nullptr);

  // `saved` false marks a project that has no file yet
  void set(const QString &name, const QString &path, bool dirty, bool saved);

  // The project has just been written to its file: the dot for unsaved
  // changes turns into a tick, with "Saved" next to it, and both leave after
  // a moment.
  void flash_saved();

  // the widest the bar can give; a longer name is shortened in the middle
  void set_max_width(int width);

  // width for the content as it is shown right now, mid-transition included
  int natural_width() const;

  bool renaming() const { return this->editing; }

  // Enter commits, Escape or a click elsewhere cancels
  void begin_rename(const QString &current, std::function<void(const QString &)> commit);

  std::function<void(const QPoint &global_pos)> on_clicked;
  std::function<void()>                         on_layout;

protected:
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;
  bool eventFilter(QObject *watched, QEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private:
  // where each part stands, left to right
  struct Parts
  {
    qreal name_x, name_w, word_x, word_w, mark_x, mark_w, chevron_x;
  };

  QFont name_font() const;
  QFont hint_font() const;

  qreal mark_size() const; // the dot is 6 across, the tick 9
  qreal mark_slot() const;
  qreal name_room() const; // what the name may take next to everything else
  qreal content() const;
  qreal natural() const;
  Parts parts() const;

  void retarget(bool animate); // every animated length to where the state wants it
  void relayout();
  void place_editor();
  void finish_rename(bool commit);
  void animate_open(bool open);

  QString name = "Untitled", prev_name;
  QString word, prev_word; // the note after the name
  QString path;
  bool    dirty = false;
  bool    saved = false;
  bool    flashing = false;   // showing "Saved"
  bool    editing = false;    // the name is being typed
  bool    committing = false; // inside the rename callback
  bool    hovered = false;
  bool    pressed = false;
  int     max_width = 460;

  Anim name_w;      // width of the name
  Anim name_t{1.0}; // 0 the previous name, 1 this one
  Anim word_w;      // width of the note, with its gap
  Anim word_t{1.0}; // 0 the previous note, 1 this one
  Anim mark_t;      // the dot or tick is there
  Anim check_t;     // 0 dot, 1 tick
  Anim edit_t;      // 0 title, 1 text field
  Anim hover_t;     // 0 at rest, 1 under the pointer
  Anim press_t;     // 1 while held
  Anim open_t;      // chevron turn: 0 down, 1 up while the menu is open

  QTimer     saved_note; // how long "Saved" stays
  QLineEdit *editor = nullptr;

  std::function<void(const QString &)> on_commit;
};

} // namespace hesiod
