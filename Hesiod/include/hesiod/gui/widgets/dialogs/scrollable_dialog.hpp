/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QDialog>
#include <QDialogButtonBox>
#include <QRect>
#include <QScrollArea>

namespace hesiod
{

/** @brief A dialog whose content scrolls and which never opens larger than the
 *  screen.
 *
 * A settings pane grows over time and grows again with the interface scale. Put
 * one straight into a QDialog and the dialog takes its content's size hint: at
 * 200% on a laptop that is taller than the display, so the bottom rows and the
 * OK button are off-screen with no way to reach them -- a dialog cannot be
 * resized past the desktop and the content has no scrollbar of its own.
 *
 * So: the content goes inside a widget-resizable QScrollArea, the button box
 * stays outside it (always reachable, never scrolled away), and the initial size
 * is clamped to the screen's *available logical* geometry, which is already the
 * scaled-down rectangle at a high QT_SCALE_FACTOR. Both scrollbars are on
 * demand, so content that is too wide is pannable rather than clipped.
 *
 * Deliberately a widget rather than a helper function: fit_to() takes the
 * rectangle explicitly so the clamping can be tested against a synthetic screen
 * instead of whatever monitor the test happens to run on.
 */
class ScrollableDialog : public QDialog
{
  Q_OBJECT

public:
  /** @param content taken over by the dialog's scroll area
   *  @param buttons which standard buttons to show outside the scroll area
   */
  ScrollableDialog(QWidget                          *content,
                   const QString                    &title,
                   QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok,
                   QWidget                          *parent = nullptr);

  QScrollArea      *scroll_area() const;
  QDialogButtonBox *button_box() const;
  QWidget          *content() const;

  /** @brief Size the dialog to its content, clamped to @p available.
   *
   * Leaves a margin so the dialog does not sit flush against the screen edges,
   * and re-centres it inside @p available if clamping moved it off.
   */
  void fit_to(const QRect &available);

  /// The screen rectangle fit_to() would use if called now.
  QRect available_screen_rect() const;

protected:
  void showEvent(QShowEvent *event) override;

private:
  QScrollArea      *scroll = nullptr;
  QDialogButtonBox *buttons = nullptr;
  QWidget          *body = nullptr;
  bool              fitted = false;
};

} // namespace hesiod
