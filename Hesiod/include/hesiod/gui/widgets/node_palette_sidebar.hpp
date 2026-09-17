/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <map>
#include <string>
#include <vector>

#include <QAbstractButton>
#include <QColor>
#include <QLineEdit>
#include <QMenu>
#include <QPointer>
#include <QVariantAnimation>
#include <QWidget>

#include "hesiod/gui/node_palette_style.hpp"

namespace hesiod
{

// =====================================
// CategoryRailButton
// =====================================

/** @brief One category in the vertical rail: glyph, label, hover/open state.
 *
 * Custom-painted rather than a styled QPushButton because the resting/hover
 * cross-fade has to be interruptible and has to settle *immediately* when
 * animations are switched off, which a stylesheet :hover cannot express.
 */
class CategoryRailButton : public QAbstractButton
{
  Q_OBJECT

public:
  CategoryRailButton(const QString          &category,
                     const QColor           &accent,
                     const NodePaletteStyle &style,
                     QWidget                *parent = nullptr);

  void set_style(const NodePaletteStyle &style);

  /// True while this category's flyout is open; drives the selected surface.
  void set_open(bool state);
  bool is_open() const;

  /// Current surface colour, i.e. where the cross-fade actually is.
  QColor current_surface() const;

  QSize sizeHint() const override;

signals:
  /// Pointer entered the button. The rail uses this to switch open flyouts.
  void hovered();

protected:
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private:
  QColor target_surface() const;
  void   retarget_surface();

  QString           category;
  QColor            accent;
  NodePaletteStyle  style;
  bool              open = false;
  bool              hovering = false;
  QColor            surface;
  QVariantAnimation surface_animation;
};

// =====================================
// NodePaletteSidebar
// =====================================

/** @brief Category rail with hierarchical flyouts, as an alternative to the
 *  dense node library tree.
 *
 * Built from the application's own node inventory (node type -> "Cat/Sub/..."),
 * so it cannot drift out of sync with the registry: every node reachable in the
 * tree is reachable here. It emits the same three activation signals as
 * NodeLibraryWidget and is wired to the same node-creation slots, so both
 * sidebars create nodes through one code path.
 *
 * Flyouts are QMenus rather than hand-rolled popups. That is what buys
 * screen-bounds clamping, scrolling when a category is taller than the display,
 * Escape/arrow navigation and Qt's sloppy-submenu tracking (no diagonal dead
 * zone) without reimplementing any of it.
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
                     const NodePaletteStyle                   &style,
                     QWidget                                  *parent = nullptr);
  ~NodePaletteSidebar() override;

  // --- Style
  void                    set_style(const NodePaletteStyle &style);
  const NodePaletteStyle &get_style() const;

  /// Restyle every sidebar alive in this process (settings changed).
  static void restyle_all(const NodePaletteStyle &style);

  // --- Inspection (also the test surface)
  QStringList              category_names() const;
  int                      category_count() const;
  CategoryRailButton      *button_at(int index) const;
  QMenu                   *menu_at(int index) const;
  std::vector<std::string> node_types_at(int index) const;
  /// Every node type reachable through the rail, sorted.
  std::vector<std::string> all_node_types() const;
  QLineEdit               *search_field() const;

  // --- Interaction
  void open_category(int index);
  void close_flyout();
  int  open_category_index() const;

protected:
  /// Watches the open flyout so moving the pointer back onto the rail switches
  /// categories: while a QMenu holds the mouse grab the rail buttons never see
  /// an enter event of their own.
  bool eventFilter(QObject *watched, QEvent *event) override;

signals:
  void node_type_selected(const std::string &node_type);
  void node_type_selected_ctrl(const std::string &node_type);
  void node_type_selected_shift(const std::string &node_type);

private:
  void   build_rail(const std::map<std::string, std::string> &inventory);
  void   setup_search(const std::map<std::string, std::string> &inventory);
  void   emit_for_type(const QString &node_type);
  void   apply_menu_style(QMenu *menu, const QColor &accent);
  QColor accent_for(const QString &category) const;

  std::map<std::string, QColor> category_colors;
  NodePaletteStyle              style;

  QWidget   *rail = nullptr;
  QLineEdit *search = nullptr;

  std::vector<CategoryRailButton *>   buttons;
  std::vector<QPointer<QMenu>>        menus;
  std::vector<std::vector<std::string>> node_types; // per category, sorted

  /// search display string -> node type
  std::map<QString, QString> search_index;

  int open_index = -1;
};

} // namespace hesiod
