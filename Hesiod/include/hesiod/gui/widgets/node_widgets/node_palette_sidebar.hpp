/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <QAction>
#include <QColor>
#include <QLineEdit>
#include <QPointer>
#include <QRectF>
#include <QScrollArea>
#include <QWidget>

#include "hesiod/gui/motion/motion.hpp"
#include "hesiod/gui/theme/node_palette_palette.hpp"
#include "hesiod/gui/widgets/node_widgets/node_palette/category_rail_button.hpp"

class QStandardItemModel;

namespace hesiod
{

class FoldButton;
class FoldFlight;
class PaletteFlyoutCard;
struct PaletteTreeNode;
class RailScroller;
class SearchFadeVeil;

/** @brief Category rail with hierarchical flyouts, as an alternative to the
 *  dense node library tree.
 *
 * Built from the application's own node inventory (node type -> "Cat/Sub/..."),
 * so it cannot drift out of sync with the registry: every node reachable in the
 * tree is reachable here. It emits the same three activation signals as
 * NodeLibraryWidget and is wired to the same node-creation slots, so both
 * sidebars create nodes through one code path.
 *
 * A category's nodes are listed in a card floating beside the rail
 * (PaletteFlyoutCard), with its sub-categories in cards of the same kind. The
 * sidebar closes it on a click elsewhere, Escape, or the window going inactive,
 * and hands it the arrow keys while it is open.
 *
 * In the graph editor it is a card floating over the graph (overlay mode): it
 * folds to its glyphs and back, slides in and out with View > Node Library,
 * and the graph's toolbar keeps to its right edge (footprint_changed).
 */
class NodePaletteSidebar : public QWidget
{
  Q_OBJECT

public:
  /** @param inventory node type -> slash-separated category path
   *  @param category_colors top-level category -> accent; unlisted categories
   *         get a deterministic hue derived from their name
   */
  NodePaletteSidebar(const std::map<std::string, std::string> &inventory,
                     const std::map<std::string, QColor>      &category_colors,
                     const NodePalettePalette                 &style,
                     QWidget                                  *parent = nullptr);
  ~NodePaletteSidebar() override;

  // --- Style
  void                      set_style(const NodePalettePalette &style);
  const NodePalettePalette &get_style() const;

  /// Restyle every sidebar alive in this process (settings changed).
  static void restyle_all(const NodePalettePalette &style);

  // --- Inspection (also the test surface)
  QStringList         category_names() const;
  int                 category_count() const;
  CategoryRailButton *button_at(int index) const;
  /// The category's nodes and sub-categories, as its card lists them.
  const PaletteTreeNode *tree_at(int index) const;
  /// The card of the open category; null before one has been opened.
  PaletteFlyoutCard       *flyout_card() const;
  std::vector<std::string> node_types_at(int index) const;
  /// Every node type reachable through the rail, sorted.
  std::vector<std::string> all_node_types() const;
  QLineEdit               *search_field() const;

  // --- Interaction
  void open_category(int index);
  void close_flyout();
  int  open_category_index() const;

  // --- Folding and overlay presentation
  /// Folded, the rail shows the category glyphs alone and takes a fraction of
  /// the width; the labels come back on unfolding. The flyouts are the same.
  void set_compact(bool state, bool animate = true);
  bool is_compact() const;

  /// Float the card over its parent (the graph canvas) instead of taking a
  /// place in a layout, so folding or hiding it never resizes the graph view.
  void set_overlay_mode(bool enabled);
  void sync_overlay_bounds();
  /// The right edge of the card as shown now, in the parent's coordinates.
  int footprint_right() const;

  /// A widget that keeps to the sidebar's right edge (the graph's toolbar). It
  /// travels with the fold as part of the same moving picture.
  void set_companion(QWidget *widget);

  /// Slide the card in or out (View > Node Library).
  void set_visible_animated(bool visible, bool animate = true);

  /// Fold or unfold every sidebar alive in this process (one per graph tab).
  static void set_all_compact(bool state);

signals:
  /// The rail was folded or unfolded, by its button or by set_compact().
  void compact_changed(bool state);

  /// The card's right edge moved (fold, unfold, slide in or out), so a docked
  /// toolbar can keep beside it.
  void footprint_changed(int right_px);

  void node_type_selected(const std::string &node_type);
  void node_type_selected_ctrl(const std::string &node_type);
  void node_type_selected_shift(const std::string &node_type);

protected:
  /// While a card is open it watches the application: a click outside the
  /// rail and the cards, Escape, or the window going inactive closes it, and
  /// the arrow keys and Return go to it. In overlay mode it also follows the
  /// parent's size.
  bool eventFilter(QObject *watched, QEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  void   build_rail(const std::map<std::string, std::string> &inventory);
  void   setup_search(const std::map<std::string, std::string> &inventory);
  void   emit_for_type(const QString &node_type);
  void   refresh_search_icons();
  void   watch_application(bool state);
  QColor accent_for(const QString &category) const;
  int    header_height() const;
  int    expanded_width() const;
  int    compact_width() const;
  int    ideal_overlay_height() const;
  void   layout_header();
  void   apply_reveal(double amount);
  void   apply_visibility(double amount);
  QRectF card_rect() const;
  bool   fly_fold(double target);
  void   land_fold();

  std::map<std::string, QColor> category_colors;
  NodePalettePalette            style;

  QWidget        *header_bar = nullptr;
  QWidget        *search_clip = nullptr;
  SearchFadeVeil *search_veil = nullptr;
  QWidget        *rail = nullptr;
  QScrollArea    *scroll = nullptr;
  RailScroller   *scroller = nullptr;
  QLineEdit      *search = nullptr;
  QAction        *search_icon = nullptr;
  FoldButton     *fold_button = nullptr;

  bool   compact = false;
  bool   overlay_mode = false;
  bool   target_visible = true;
  double reveal = 1.0;     // 0 folded .. 1 unfolded
  double visibility = 1.0; // 0 slid out .. 1 shown
  Anim   reveal_anim{1.0};
  Anim   visibility_anim{1.0};

  // the fold as one moving picture, see fly_fold()
  QPointer<QWidget>    companion;
  QPointer<FoldFlight> flight;
  bool                 content_only = false; // while a picture of the content is taken
  bool                 flying = false; // the widgets wait, hidden, under the picture
  bool                 search_wants_focus = false;

  std::vector<CategoryRailButton *>             buttons;
  std::vector<std::shared_ptr<PaletteTreeNode>> trees;
  std::vector<std::vector<std::string>>         node_types; // per category, sorted
  QPointer<PaletteFlyoutCard>                   flyout;
  bool                                          watching_application = false;

  /// search display string -> node type; the model holds the same rows with
  /// a glyph each
  std::map<QString, QString> search_index;
  QStandardItemModel        *search_model = nullptr;

  int open_index = -1;
};

} // namespace hesiod
