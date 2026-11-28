/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <functional>
#include <vector>

#include <QDialog>
#include <QPixmap>
#include <QPoint>
#include <QVector>

#include "hesiod/gui/motion/motion.hpp"

class QKeyEvent;
class QMouseEvent;
class QPainter;
class QRectF;
class QResizeEvent;
class QScrollArea;
class QShowEvent;
class QVBoxLayout;
class QWidget;

namespace hesiod
{

class ExampleTile;
class ToggleSwitch;
class WindowButton;

/**
 * @brief The start screen: what Hesiod shows before a project is open.
 *
 * One page. On the left a quiet rail with the name of the application, the
 * two ways to begin (a new project, a project file) and the projects opened
 * lately. On the right the example terrains as pictures, the first one large:
 * the pictures are the invitation, so nothing is drawn around them.
 *
 * It arrives in one short composed movement: the card first, then the rail
 * and the pictures a beat apart from each other. With interface animations
 * off everything is in place at once.
 */
class ExampleSelectorDialog : public QDialog
{
  Q_OBJECT

public:
  explicit ExampleSelectorDialog(const QString &examples_path, QWidget *parent = nullptr);
  ~ExampleSelectorDialog() override;

  /** @brief What the user actually asked for.
   *
   * A boolean accepted/rejected cannot carry this. "New Project" and closing
   * the window both left the dialog rejected, so a caller could not tell them
   * apart: from the menu bar New Project closed the window and left the open
   * project untouched, and closing the window could not exit the app because
   * that would also have fired on New Project.
   */
  enum class Outcome
  {
    Closed,     ///< dismissed without choosing; the app should not continue
    NewProject, ///< start an empty project
    OpenFile    ///< load selected_file()
  };

  Outcome outcome() const;
  QString selected_file() const;
  bool    selected_is_project() const;

protected:
  void showEvent(QShowEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;

private:
  void accept_path(const QString &path, bool project);
  void build_examples(QWidget *host);
  void build_rail(QWidget *host);
  void layout_tiles();
  void on_reject();
  void open_project();
  void paint_brand(QPainter &p, const QRectF &card);
  void rebuild_recent_projects();
  void start_new_project();
  void start_reveal();

  QString examples_path;
  QString selected_filename;
  bool    selected_project = false;
  Outcome result = Outcome::Closed;

  QPixmap                lockup; // the mark and the name, as one picture
  WindowButton          *close_button = nullptr;
  QVBoxLayout           *recent_layout = nullptr;
  QWidget               *recent_host = nullptr;
  QScrollArea           *scroll_area = nullptr;
  QWidget               *tiles_host = nullptr;
  ToggleSwitch          *startup_toggle = nullptr;
  QWidget               *first_action = nullptr;
  QVector<ExampleTile *> tiles;

  // the entrance: 0 not yet there, 1 in place
  Anim brand_t{this};
  // each part of the rail, in order: call with a delay to have it arrive
  std::vector<std::function<void(int)>> action_arrivals;
  std::vector<std::function<void(int)>> recent_arrivals;
  bool                                  revealed = false;

  // the card is dragged by any part of it that is not a control
  bool   dragging = false;
  QPoint drag_offset;
};

} // namespace hesiod
