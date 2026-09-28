/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QDialog>
#include <QIcon>
#include <QPoint>

class QAbstractButton;
class QApplication;
class QGridLayout;
class QHBoxLayout;
class QLabel;
class QPushButton;
class QVBoxLayout;

namespace hesiod
{

// =====================================
// MessageDialog
// =====================================

// Prompt in the application's own chrome (frameless, rounded, dark), for the
// questions the user meets most: crash recovery, unsaved changes, settings.
// Plays the role of a QMessageBox with explicit button roles, so the one that
// matters stands out: Primary is filled with the accent, Danger reads red,
// Secondary is quiet.
class MessageDialog : public QDialog
{
public:
  enum class Kind
  {
    Info,
    Question,
    Warning,
    Error,
    Restore,
    None // no badge: title and content only
  };

  enum class Role
  {
    Primary,
    Secondary,
    Danger
  };

  MessageDialog(QWidget *parent, Kind kind, const QString &title, const QString &text);

  // key / value line in the details card (e.g. "File", a path)
  void add_detail(const QString &key, const QString &value);

  // small print below the details
  void set_footnote(const QString &text);

  // custom content, placed between the text and the details card
  QVBoxLayout *body() const { return this->body_layout; }

  // card width in logical pixels (default 460)
  void set_card_width(int width);

  // Buttons appear in the order they are added; Danger buttons (and buttons
  // that do not close the dialog) go to the left. The escape button rejects
  // the dialog, every other closing button accepts it.
  QPushButton *add_button(const QString &label,
                          Role           role,
                          bool           is_default = false,
                          bool           is_escape = false,
                          bool           closes = true);

  QAbstractButton *clicked_button() const { return this->clicked; }

  // painted badge shared with the native message box polish
  static QPixmap badge(Kind kind, int size, qreal dpr);

protected:
  void keyPressEvent(QKeyEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void showEvent(QShowEvent *event) override;

private:
  QVBoxLayout     *text_layout = nullptr;
  QVBoxLayout     *body_layout = nullptr;
  QLabel          *icon = nullptr;
  int              card_width = 460;
  QGridLayout     *details = nullptr;
  QWidget         *details_card = nullptr;
  QLabel          *footnote = nullptr;
  QHBoxLayout     *danger_layout = nullptr;
  QHBoxLayout     *buttons_layout = nullptr;
  QAbstractButton *clicked = nullptr;
  QAbstractButton *escape = nullptr;
  QPoint           drag_offset;
  bool             dragging = false;
};

// Application-wide polish for the windows Hesiod does not draw itself
// (QMessageBox, colour/input dialogs, the satellite tool windows): a dark
// native caption in the application's colours, themed message-box icons and
// accent / danger emphasis on the dialog buttons.
void install_native_dialog_polish(QApplication &app);

} // namespace hesiod
