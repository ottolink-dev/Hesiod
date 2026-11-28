/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once
#include <functional>
#include <memory>
#include <vector>

#include <QPointer>
#include <QTimer>
#include <QWidget>

#include "hesiod/gui/motion/motion.hpp"
#include "hesiod/gui/theme/node_palette_palette.hpp"
#include "hesiod/gui/widgets/node_widgets/node_palette/palette_tree_node.hpp"

namespace hesiod
{

/** @brief The list of a category's nodes, floating beside the node library.
 *
 * A frameless tool window drawn by hand: one highlight that glides from row to
 * row, its own scrolling and a quiet scrollbar for a category taller than the
 * card, and sub-categories that open a card of the same kind beside it. While
 * a card is open, moving to another category moves the card there instead of
 * closing one and opening the next.
 *
 * It never takes the keyboard focus; the library forwards the keys to it
 * (key()) and closes it on a click elsewhere, Escape or the window going
 * inactive.
 */
class PaletteFlyoutCard : public QWidget
{
  Q_OBJECT

public:
  /// @param on_pick called with the node type of the row clicked
  PaletteFlyoutCard(const NodePalettePalette            &style,
                    std::function<void(const QString &)> on_pick,
                    QWidget                             *owner = nullptr);
  ~PaletteFlyoutCard() override;

  void set_style(const NodePalettePalette &style);

  /// Show @p node beside @p anchor_global (a point on the edge it opens from),
  /// on the left of it when @p prefer_left or when the screen ends first.
  void present(const std::shared_ptr<PaletteTreeNode> &node,
               const QPoint                           &anchor_global,
               bool                                    prefer_left = false);
  void dismiss();

  bool                   is_open() const;
  const PaletteTreeNode *node() const;
  /// This card or one of the sub-category cards it opened covers the point.
  bool contains_global(const QPoint &global_pos) const;
  /// The rows are taller than the card and scroll.
  bool scrolls() const;
  /// The sub-category card, once one has been opened.
  PaletteFlyoutCard *child_card() const;

  /// Up, Down, Return, Right and Left, for the deepest open card. False when
  /// the key is not for the cards (Left on the first card closes the category
  /// on the rail instead).
  bool key(int key);

  /// What a click on a node's row does.
  void pick(const QString &node_type);

protected:
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;

private:
  struct Row
  {
    bool                             separator = false;
    QString                          label;
    QString                          icon; // node type, or the sub-category's path
    std::shared_ptr<PaletteTreeNode> submenu;
    int                              count = 0;
    qreal                            y = 0.0;
    qreal                            h = 0.0;
  };

  bool   animated() const;
  int    row_height() const;
  int    padding() const;
  qreal  card_radius() const;
  QRectF card_rect() const;
  QRectF list_rect() const;
  qreal  max_scroll() const;
  void   rebuild_rows();
  QRect  compute_geometry(const QPoint &anchor_global, bool prefer_left);
  int    row_at_pos(const QPointF &pos) const;
  void   set_active_row(int row);
  void   step_row(int direction);
  void   scroll_to_row(int row);
  void   commit_pending_child();
  void   paint_shadow(QPainter &painter, const QRectF &card, qreal alpha) const;
  void   paint_row(QPainter     &painter,
                   const Row    &row,
                   const QRectF &row_rect,
                   bool          active) const;

  NodePalettePalette                   style;
  std::function<void(const QString &)> on_pick;
  std::shared_ptr<PaletteTreeNode>     current_node;
  std::vector<Row>                     rows;
  qreal                                total_content_h = 0.0;
  bool                                 open_state = false;
  bool                                 opened_left = false;
  bool                                 hovered = false;
  int                                  active_row = -1;
  int                                  open_child_row = -1;
  int                                  pending_child_row = -1;

  QRect geom_from;
  QRect geom_to;
  Anim  appear_t;
  Anim  geom_t;
  Anim  content_t;
  Anim  highlight_y;
  Anim  highlight_alpha;
  Anim  scroll_y;
  Anim  scrollbar_alpha;

  QTimer                      sub_open_timer;
  QTimer                      sub_close_timer;
  QTimer                      scrollbar_timer;
  QPointer<PaletteFlyoutCard> child;
};

} // namespace hesiod
