/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>

#include <QAbstractItemView>
#include <QApplication>
#include <QCompleter>
#include <QDir>
#include <QEnterEvent>
#include <QPainter>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <QCheckBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

#include "hesiod/app/ui_scale.hpp"
#include "hesiod/gui/widgets/node_palette_sidebar.hpp"
#include "hesiod/gui/widgets/scrollable_dialog.hpp"

using namespace hesiod;

// --- tiny harness, same shape as the gnodegui node_style test

namespace
{
int g_failures = 0;

void check(bool condition, const std::string &what)
{
  if (condition)
  {
    std::cout << "  ok   " << what << "\n";
  }
  else
  {
    std::cout << "  FAIL " << what << "\n";
    ++g_failures;
  }
}

void section(const std::string &name) { std::cout << "\n[" << name << "]\n"; }

/// A stand-in inventory shaped exactly like get_node_inventory(): node type ->
/// slash-separated category path, with a deep path, a category-root node and a
/// category that the colour map does not name.
std::map<std::string, std::string> sample_inventory()
{
  return {
      {"Abs", "Math/Base"},
      {"AbsSmooth", "Math/Base"},
      {"Blend", "Operator/Blend"},
      {"Blend3", "Operator/Blend"},
      {"BlenderBridge", "Bridges"}, // node sitting directly on a category
      {"Bulkify", "Boundaries"},    // category with no entry in the colour map
      {"Clamp", "Filter/Range"},
      {"Remap", "Filter/Range"},
      {"FillTalus", "Filter/Advanced Filters"},
      {"Thermal", "Erosion/Deposition"},
      {"ValleyFill", "Erosion/Deposition"},
      {"HydraulicParticle", "Erosion/Hydraulic"},
      {"Strata", "Erosion/Stratify"},
      {"AreaRemove", "Terrain Features/Morphology"},
      {"CombineMask", "Terrain Features/Mask Operations"},
      {"ExportHeightmap", "Export"},
      {"Preview", "Debug"},
      {"Noise", "Primitive/Coherent"},
      {"CellularNoise", "Primitive/Coherent"},
      {"Band", "Primitive/Function"},
      {"Brush", "Primitive/Authoring"},
  };
}

std::map<std::string, QColor> sample_colors()
{
  return {{"Math", QColor(0, 43, 54)}, // near black in the real registry
          {"Operator", QColor(108, 113, 196)},
          {"Filter", QColor(108, 113, 196)},
          {"Erosion", QColor(203, 75, 22)},
          {"Debug", QColor(200, 0, 0)},
          {"Primitive", QColor(42, 161, 152)}};
}

QAction *find_action(QMenu *menu, const QString &text)
{
  for (QAction *action : menu->actions())
  {
    if (action->menu())
    {
      if (QAction *hit = find_action(action->menu(), text))
        return hit;
    }
    else if (action->text() == text)
    {
      return action;
    }
  }
  return nullptr;
}

int count_leaf_actions(QMenu *menu)
{
  int n = 0;
  for (QAction *action : menu->actions())
  {
    if (action->menu())
      n += count_leaf_actions(action->menu());
    else if (!action->isSeparator())
      ++n;
  }
  return n;
}

void write_config(const QString &path, const std::string &body)
{
  std::ofstream file(path.toStdString());
  file << body;
}

} // namespace

// =====================================
// interface scale
// =====================================

void test_scale_sanitizing()
{
  section("interface scale: validation");

  bool clean = false;

  check(ui_scale::sanitize(1.25, &clean) == 1.25 && clean, "an in-range factor is kept");

  check(ui_scale::sanitize(std::numeric_limits<double>::quiet_NaN()) ==
            ui_scale::kDefault,
        "NaN falls back to 1.00");
  check(ui_scale::sanitize(std::numeric_limits<double>::infinity()) ==
            ui_scale::kDefault,
        "infinity falls back to 1.00");
  check(ui_scale::sanitize(-std::numeric_limits<double>::infinity()) ==
            ui_scale::kDefault,
        "negative infinity falls back to 1.00");

  check(ui_scale::sanitize(9.0, &clean) == ui_scale::kMax && !clean,
        "a too-large factor clamps to the maximum and is reported as corrected");
  check(ui_scale::sanitize(0.01, &clean) == ui_scale::kMin && !clean,
        "a too-small factor clamps to the minimum");
  check(ui_scale::sanitize(-4.0) == ui_scale::kMin, "a negative factor clamps up");
  check(ui_scale::sanitize(0.0) == ui_scale::kMin, "zero clamps up");

  // the property that makes repeated changes safe: sanitize is idempotent, so
  // storing and re-reading a value can never walk it anywhere
  for (const double value : {0.5, 0.83, 1.0, 1.25, 2.4, 3.0})
    check(ui_scale::sanitize(ui_scale::sanitize(value)) == ui_scale::sanitize(value),
          "sanitize is idempotent for " + std::to_string(value));
}

void test_scale_never_compounds()
{
  section("interface scale: repeated changes and reset");

  QTemporaryDir dir;
  const QString config = dir.filePath("hesiod.json");

  // a settings round trip is: sanitize on write, sanitize on read. Doing it
  // repeatedly must land exactly where a single jump would.
  double running = ui_scale::kDefault;

  for (const double requested : {1.25, 1.5, 0.75, 2.0, 2.0, 1.0})
  {
    write_config(config,
                 "{\"app_settings\": {\"interface.ui_scale\": " +
                     std::to_string(ui_scale::sanitize(requested)) + "}}");
    running = ui_scale::scale_from_config(config.toStdString());

    check(std::abs(running - requested) < 1e-9,
          "setting " + std::to_string(requested) + " reads back unchanged");
  }

  check(std::abs(running - ui_scale::kDefault) < 1e-9,
        "reset to 1.00 after five changes lands exactly on 1.00");

  // the same sequence applied in one step gives the same answer
  write_config(config, "{\"app_settings\": {\"interface.ui_scale\": 2.0}}");
  const double direct = ui_scale::scale_from_config(config.toStdString());
  write_config(config, "{\"app_settings\": {\"interface.ui_scale\": 2.0}}");
  const double after_history = ui_scale::scale_from_config(config.toStdString());
  check(direct == after_history, "the factor does not depend on the previous value");
}

void test_scale_malformed_config()
{
  section("interface scale: malformed persisted values");

  QTemporaryDir dir;
  const QString config = dir.filePath("hesiod.json");

  check(ui_scale::scale_from_config(dir.filePath("nope.json").toStdString()) ==
            ui_scale::kDefault,
        "a missing config gives 1.00");

  check(ui_scale::scale_from_config("") == ui_scale::kDefault,
        "an empty path gives 1.00");

  write_config(config, "{ this is not json");
  check(ui_scale::scale_from_config(config.toStdString()) == ui_scale::kDefault,
        "a truncated config gives 1.00 instead of throwing");

  write_config(config, "{}");
  check(ui_scale::scale_from_config(config.toStdString()) == ui_scale::kDefault,
        "a config without app_settings gives 1.00");

  write_config(config, "{\"app_settings\": {}}");
  check(ui_scale::scale_from_config(config.toStdString()) == ui_scale::kDefault,
        "a config without the key gives 1.00");

  write_config(config, "{\"app_settings\": {\"interface.ui_scale\": \"huge\"}}");
  check(ui_scale::scale_from_config(config.toStdString()) == ui_scale::kDefault,
        "a string factor gives 1.00");

  write_config(config, "{\"app_settings\": {\"interface.ui_scale\": null}}");
  check(ui_scale::scale_from_config(config.toStdString()) == ui_scale::kDefault,
        "a null factor gives 1.00");

  write_config(config, "{\"app_settings\": {\"interface.ui_scale\": {\"x\": 2}}}");
  check(ui_scale::scale_from_config(config.toStdString()) == ui_scale::kDefault,
        "an object factor gives 1.00");

  write_config(config, "{\"app_settings\": {\"interface.ui_scale\": 1e9}}");
  check(ui_scale::scale_from_config(config.toStdString()) == ui_scale::kMax,
        "an absurd factor clamps to the maximum rather than making the app unusable");

  write_config(config, "{\"app_settings\": {\"interface.ui_scale\": -2}}");
  check(ui_scale::scale_from_config(config.toStdString()) == ui_scale::kMin,
        "a negative factor clamps to the minimum");

  write_config(config, "{\"app_settings\": {\"interface.ui_scale\": 1.4}}");
  check(std::abs(ui_scale::scale_from_config(config.toStdString()) - 1.4) < 1e-9,
        "a good factor still reads back");
}

void test_scale_environment_override()
{
  section("interface scale: environment override");

  const QByteArray original = qgetenv("QT_SCALE_FACTOR");
  const bool       had_original = qEnvironmentVariableIsSet("QT_SCALE_FACTOR");

  qunsetenv("QT_SCALE_FACTOR");
  {
    const ui_scale::Resolution r = ui_scale::apply_scale(1.5);
    check(!r.environment_override, "with nothing in the environment there is no override");
    check(r.configured == 1.5 && r.effective == 1.5,
          "the preference is both configured and effective");
    check(qgetenv("QT_SCALE_FACTOR").toDouble() == 1.5,
          "QT_SCALE_FACTOR is set from the preference");
    check(ui_scale::session_scale() == 1.5, "session_scale reports the applied factor");
  }

  // the misleading case: an override is in force, so the preference is *not*
  // what the process runs at and must not be reported as if it were
  qputenv("QT_SCALE_FACTOR", "2.5");
  {
    const ui_scale::Resolution r = ui_scale::apply_scale(1.25);
    check(r.environment_override, "an existing QT_SCALE_FACTOR is flagged as an override");
    check(qgetenv("QT_SCALE_FACTOR") == QByteArray("2.5"),
          "the external value is preserved, not overwritten");
    check(r.configured == 1.25, "the preference is still reported separately");
    check(r.effective == 2.5, "the effective factor is the environment's");
    check(ui_scale::session_scale() == 2.5,
          "session_scale no longer claims the preference is active");
    check(ui_scale::session_resolution().environment_override,
          "the override is visible to the settings dialog");
  }

  // Qt ignores a factor it cannot use, so the process really does run at 1.00
  qputenv("QT_SCALE_FACTOR", "not-a-number");
  {
    const ui_scale::Resolution r = ui_scale::apply_scale(2.0);
    check(r.environment_override, "an unparseable override is still an override");
    check(r.effective == ui_scale::kDefault,
          "an unparseable override means the process runs at 1.00");
    check(qgetenv("QT_SCALE_FACTOR") == QByteArray("not-a-number"),
          "an unparseable override is still left alone");
  }

  qputenv("QT_SCALE_FACTOR", "0");
  check(ui_scale::apply_scale(2.0).effective == ui_scale::kDefault,
        "a zero override means 1.00");

  qputenv("QT_SCALE_FACTOR", "-1");
  check(ui_scale::apply_scale(2.0).effective == ui_scale::kDefault,
        "a negative override means 1.00");

  qputenv("QT_SCALE_FACTOR", "5");
  {
    const ui_scale::Resolution r = ui_scale::apply_scale(1.0);
    check(r.effective == 5.0,
          "an override beyond our supported range is reported as-is, not clamped");
    check(qgetenv("QT_SCALE_FACTOR") == QByteArray("5"),
          "and is not rewritten into our range");
  }

  // A fractional factor has to survive the round trip through the environment
  // exactly. 'g' formatting can emit an exponent, and a non-C locale can emit a
  // comma, both of which Qt reparses as something else entirely -- 0.9 arriving
  // as 0 or 9 is the difference between a slightly smaller interface and an
  // unusable one.
  qunsetenv("QT_SCALE_FACTOR");
  for (const double factor : {0.5, 0.75, 0.9, 1.05, 1.25, 2.5, 3.0})
  {
    ui_scale::apply_scale(factor);

    const QByteArray written = qgetenv("QT_SCALE_FACTOR");

    check(std::abs(written.toDouble() - factor) < 1e-9,
          "a fractional scale reaches Qt unchanged");
    check(!written.contains('e') && !written.contains('E'),
          "the factor is never written in exponent form");
    check(!written.contains(','),
          "the factor never carries a locale decimal comma");

    qunsetenv("QT_SCALE_FACTOR");
  }

  if (had_original)
    qputenv("QT_SCALE_FACTOR", original);
  else
    qunsetenv("QT_SCALE_FACTOR");

  ui_scale::apply_scale(ui_scale::kDefault);
}

void test_executable_path()
{
  section("interface scale: executable path");

  const QString dir = QString::fromStdString(ui_scale::executable_dir(nullptr));

  check(!dir.isEmpty(), "the executable directory resolves without argv[0]");
  check(QDir(dir).exists(), "it names a directory that exists");
  check(QDir(dir).canonicalPath() ==
            QDir(QFileInfo(QCoreApplication::applicationFilePath()).absolutePath())
                .canonicalPath(),
        "it matches what Qt reports once the application exists");

  // a launch through PATH hands us a bare name; resolving that against the
  // working directory would look for the portable config in the wrong place
  check(QString::fromStdString(ui_scale::executable_dir("hesiod.exe")) == dir,
        "a bare argv[0] does not move the resolved directory");
  check(QString::fromStdString(ui_scale::executable_dir("../../hesiod.exe")) == dir,
        "a relative argv[0] does not move it either");

  check(QString::fromStdString(ui_scale::startup_config_path(nullptr))
            .endsWith("hesiod.json"),
        "the resolved config path names hesiod.json");
}

// =====================================
// settings dialog
// =====================================

QWidget *make_settings_stub(int rows)
{
  auto *widget = new QWidget;
  auto *form = new QFormLayout(widget);
  form->setRowWrapPolicy(QFormLayout::WrapLongRows);

  for (int i = 0; i < rows; ++i)
    form->addRow(QString("A settings label of realistic length, number %1").arg(i),
                 new QCheckBox);

  return widget;
}

void test_settings_dialog()
{
  section("settings dialog: scrolling and screen cap");

  QWidget  *content = make_settings_stub(80);
  const int content_height = content->sizeHint().height();

  ScrollableDialog dialog(content, "Application Settings");

  check(dialog.scroll_area() != nullptr, "the content is inside a scroll area");
  check(dialog.scroll_area()->widgetResizable(),
        "the scroll area resizes its widget to the viewport, so rows reflow");
  check(dialog.scroll_area()->widget() == content,
        "the settings pane is the scrolled widget");
  check(dialog.button_box() != nullptr, "the dialog has a button box");
  check(!dialog.scroll_area()->isAncestorOf(dialog.button_box()),
        "OK sits outside the scroll area, so it cannot scroll out of reach");
  check(dialog.button_box()->button(QDialogButtonBox::Ok) != nullptr,
        "the OK button exists");

  // a screen far smaller than the content: this is the 200%-on-a-laptop case
  const QRect tiny(0, 0, 600, 400);

  dialog.show();
  dialog.fit_to(tiny);
  QApplication::processEvents();

  check(content_height > tiny.height(),
        "the stub content really is taller than the test screen");
  check(dialog.height() <= tiny.height(), "the dialog is not taller than the screen");
  check(dialog.width() <= tiny.width(), "the dialog is not wider than the screen");
  check(tiny.contains(dialog.geometry()),
        "the whole dialog sits inside the screen rectangle");

  QScrollBar *vbar = dialog.scroll_area()->verticalScrollBar();
  check(vbar->maximum() > 0, "the content scrolls rather than being clipped");
  vbar->setValue(vbar->maximum());
  QApplication::processEvents();
  check(vbar->value() == vbar->maximum(), "the last row can be scrolled to");

  check(dialog.button_box()->isVisible(), "OK is visible");
  check(dialog.rect().contains(dialog.button_box()->geometry()),
        "OK stays inside the dialog no matter how far the content is scrolled");

  dialog.hide();

  // horizontal overflow must be pannable, not a reason for a control to be
  // unreachable
  auto *wide = new QWidget;
  auto *wide_layout = new QHBoxLayout(wide);
  wide_layout->addWidget(new QLabel(QString(400, QChar('W'))));

  ScrollableDialog wide_dialog(wide, "Wide content");
  wide_dialog.show();
  wide_dialog.fit_to(tiny);
  QApplication::processEvents();

  check(wide_dialog.width() <= tiny.width(),
        "a pane wider than the screen does not widen the dialog past it");
  check(wide_dialog.scroll_area()->horizontalScrollBar()->maximum() > 0,
        "a pane wider than the screen can be scrolled sideways");
  check(wide_dialog.button_box()->isVisible(),
        "OK is still reachable with horizontal overflow");

  wide_dialog.hide();

  // and against the screen this process actually has, at whatever scale it is
  // running: the same clamp has to hold there, which is the case that broke
  ScrollableDialog real_dialog(make_settings_stub(80), "Application Settings");
  real_dialog.show();
  QApplication::processEvents();

  const QRect available = real_dialog.available_screen_rect();
  check(real_dialog.height() <= available.height(),
        "on this process's real screen the dialog is not taller than it");
  check(real_dialog.width() <= available.width(),
        "and not wider than it");
  check(real_dialog.button_box()->isVisible(),
        "OK is on screen at this process's interface scale");

  real_dialog.hide();
}

// =====================================
// node palette sidebar
// =====================================

void test_category_completeness()
{
  section("sidebar: categories and node coverage");

  const auto inventory = sample_inventory();

  NodePaletteSidebar sidebar(inventory, sample_colors(), NodePaletteStyle{});

  // the rail is exactly the set of distinct top-level categories
  std::set<QString> expected;
  for (const auto &[type, path] : inventory)
    expected.insert(QString::fromStdString(path).split('/').front());

  const QStringList names = sidebar.category_names();
  check(names.size() == static_cast<int>(expected.size()),
        "one rail button per top-level category");

  bool all_named = true;
  for (const QString &name : names)
    all_named &= expected.count(name) > 0;
  check(all_named, "every rail button names a real category");

  // no node may be unreachable: that is the failure mode of a hand-written menu
  const std::vector<std::string> reachable = sidebar.all_node_types();
  check(reachable.size() == inventory.size(),
        "every node in the inventory is reachable from the rail");

  bool every_node_present = true;
  for (const auto &[type, path] : inventory)
    every_node_present &= std::find(reachable.begin(), reachable.end(), type) !=
                          reachable.end();
  check(every_node_present, "no node type is missing from the flyouts");

  // and the menus themselves hold those nodes, not just the bookkeeping vectors
  int leaf_actions = 0;
  for (int i = 0; i < sidebar.category_count(); ++i)
    leaf_actions += count_leaf_actions(sidebar.menu_at(i));
  check(leaf_actions == static_cast<int>(inventory.size()),
        "the flyout menus contain exactly the inventory's nodes");

  // a deep path becomes nested submenus, not a flattened list
  const int erosion = names.indexOf("Erosion");
  check(erosion >= 0, "the Erosion category exists");
  if (erosion >= 0)
  {
    QMenu *menu = sidebar.menu_at(erosion);
    int    submenus = 0;
    for (QAction *action : menu->actions())
      if (action->menu())
        ++submenus;
    check(submenus == 3, "Erosion has one submenu per subcategory");
    check(find_action(menu, "Thermal") != nullptr,
          "a node two levels deep is present in the flyout tree");
  }

  // a category the colour map does not name still gets a usable, stable accent
  const int boundaries = names.indexOf("Boundaries");
  check(boundaries >= 0, "an uncoloured category still gets a rail button");
  if (boundaries >= 0)
    check(sidebar.node_types_at(boundaries) == std::vector<std::string>{"Bulkify"},
          "an uncoloured category still lists its nodes");

  // a node sitting directly on a category is reachable without a submenu
  const int bridges = names.indexOf("Bridges");
  if (bridges >= 0)
  {
    QMenu *menu = sidebar.menu_at(bridges);
    check(menu->actions().size() == 1 && menu->actions().front()->text() ==
                                             "BlenderBridge",
          "a node on a category root is a direct flyout entry");
  }
}

void test_node_creation_signal()
{
  section("sidebar: node creation");

  NodePaletteSidebar sidebar(sample_inventory(), sample_colors(), NodePaletteStyle{});

  QSignalSpy spy(&sidebar, &NodePaletteSidebar::node_type_selected);

  const int erosion = sidebar.category_names().indexOf("Erosion");
  QAction  *thermal = find_action(sidebar.menu_at(erosion), "Thermal");
  check(thermal != nullptr, "the Thermal action exists");

  if (thermal)
  {
    thermal->trigger();
    check(spy.count() == 1, "triggering a flyout entry requests one node");
    if (spy.count() == 1)
      check(spy.front().front().value<std::string>() == "Thermal",
            "the requested node type is the one that was picked");
  }

  // a submenu entry must work without the menu ever having been popped up along
  // its parent chain, which is how the flyouts are actually driven
  QAction *deep = find_action(sidebar.menu_at(erosion), "HydraulicParticle");
  if (deep)
  {
    deep->trigger();
    check(spy.count() == 2, "a nested entry requests a node too");
  }
}

void test_search()
{
  section("sidebar: search");

  NodePaletteSidebar sidebar(sample_inventory(), sample_colors(), NodePaletteStyle{});

  QLineEdit *field = sidebar.search_field();
  check(field != nullptr, "the rail has a search field");

  QCompleter *completer = field ? field->completer() : nullptr;
  check(completer != nullptr, "the search field is backed by a completer");

  if (!completer)
    return;

  check(completer->filterMode() == Qt::MatchContains,
        "search matches anywhere in the entry, not just as a prefix");
  check(completer->caseSensitivity() == Qt::CaseInsensitive,
        "search is case insensitive");

  completer->setCompletionPrefix("thermal");
  check(completer->completionCount() == 1, "searching a node name finds it");

  // the category is part of the searchable text, so a family search works
  completer->setCompletionPrefix("Erosion");
  check(completer->completionCount() == 4,
        "searching a category finds every node in it");

  completer->setCompletionPrefix("zzzz");
  check(completer->completionCount() == 0, "a nonsense query finds nothing");

  QSignalSpy spy(&sidebar, &NodePaletteSidebar::node_type_selected);
  field->setText("Clamp");
  QTest::keyClick(field, Qt::Key_Return);
  check(spy.count() == 1, "Enter on an exact node name creates it");
  check(field->text().isEmpty(), "the field clears after creating a node");
}

void test_flyout_geometry()
{
  section("sidebar: flyout geometry at scale");

  // a deliberately oversized style stands in for a large interface scale: the
  // flyout must still land on the screen and become scrollable rather than
  // running off the bottom
  NodePaletteStyle style;
  style.button_height = 92;
  style.flyout_row_height = 76;
  style.icon_size = 44;
  style.rail_padding = 18;

  // an inventory big enough that one category cannot fit on the test screen
  std::map<std::string, std::string> inventory;
  for (int i = 0; i < 120; ++i)
    inventory["Node" + std::to_string(i)] = "Primitive/Coherent";

  NodePaletteSidebar sidebar(inventory, sample_colors(), style);
  sidebar.resize(240, 300); // a short, narrow panel
  sidebar.show();
  QApplication::processEvents();

  const QRect available = QApplication::primaryScreen()->availableGeometry();

  sidebar.open_category(0);
  QApplication::processEvents();

  QMenu *menu = sidebar.menu_at(0);
  check(menu->isVisible(), "the flyout opened");
  check(sidebar.open_category_index() == 0, "the rail knows which flyout is open");
  check(sidebar.button_at(0)->is_open(), "the open category is marked on the rail");

  const QRect geom = menu->geometry();
  check(available.contains(geom, /*proper*/ false) ||
            (geom.top() >= available.top() && geom.bottom() <= available.bottom() &&
             geom.left() >= available.left() && geom.right() <= available.right()),
        "the flyout stays inside the available screen area");
  check(geom.height() <= available.height(),
        "the flyout is never taller than the screen (it scrolls instead)");

  sidebar.close_flyout();
  QApplication::processEvents();
  check(!menu->isVisible(), "closing the flyout hides it");
  check(sidebar.open_category_index() == -1, "the rail forgets the closed flyout");
  check(!sidebar.button_at(0)->is_open(), "the rail button returns to its resting state");

  // the rail itself has to stay reachable in a short panel
  auto *scroll = sidebar.findChild<QScrollArea *>();
  check(scroll != nullptr, "the category rail lives in a scroll area");
  if (scroll)
  {
    scroll->widget()->adjustSize();
    QApplication::processEvents();
    check(scroll->verticalScrollBar()->maximum() > 0 ||
              scroll->widget()->height() <= scroll->viewport()->height(),
          "categories that do not fit can be scrolled to");
  }

  sidebar.hide();
}

void test_animation_toggle()
{
  section("sidebar: animations");

  NodePaletteStyle style;
  style.animations = true;
  style.animation_ms = 400;

  NodePaletteSidebar sidebar(sample_inventory(), sample_colors(), style);
  sidebar.resize(240, 600);
  sidebar.show();
  QApplication::processEvents();

  CategoryRailButton *button = sidebar.button_at(0);
  check(button->current_surface() == style.surface, "a resting button uses the surface");

  auto hover = [](CategoryRailButton *b, bool on)
  {
    if (on)
    {
      QEnterEvent event(QPointF(1, 1), QPointF(1, 1), QPointF(1, 1));
      QApplication::sendEvent(b, &event);
    }
    else
    {
      QEvent event(QEvent::Leave);
      QApplication::sendEvent(b, &event);
    }
  };

  hover(button, true);
  check(button->current_surface() != style.surface_hover,
        "with animations on the hover colour is faded into, not jumped to");

  // switching animations off has to settle whatever is running, immediately
  NodePaletteStyle still = style;
  still.animations = false;
  sidebar.set_style(still);
  check(button->current_surface() == still.surface_hover,
        "disabling animations lands on the hover colour at once");

  hover(button, false);
  check(button->current_surface() == still.surface,
        "with animations off, leaving settles immediately too");

  hover(button, true);
  check(button->current_surface() == still.surface_hover,
        "and entering settles immediately too");

  sidebar.hide();
}

void test_restyle()
{
  section("sidebar: live restyle");

  NodePaletteSidebar sidebar(sample_inventory(), sample_colors(), NodePaletteStyle{});

  NodePaletteStyle style;
  style.surface = QColor("#101010");
  style.button_height = 64;
  style.button_spacing = 14;
  style.animations = false;

  const int width_before = sidebar.minimumWidth();

  NodePaletteSidebar::restyle_all(style);

  check(sidebar.get_style().surface == QColor("#101010"),
        "a settings change reaches a sidebar that is already open");
  check(sidebar.button_at(0)->current_surface() == QColor("#101010"),
        "the rail buttons repaint with the new surface");
  check(sidebar.button_at(0)->sizeHint().height() == 64,
        "spacing and sizing follow the new style");

  // restyling twice with the same values must not creep: the metrics come from
  // the style every time, they are never multiplied into the widget
  NodePaletteSidebar::restyle_all(style);
  NodePaletteSidebar::restyle_all(style);
  const int width_after = sidebar.minimumWidth();
  NodePaletteSidebar::restyle_all(style);
  check(sidebar.minimumWidth() == width_after,
        "repeated restyles converge instead of compounding");
  (void)width_before;
}

/// Suffix identifying this process's actual Qt scaling, for file names.
QString dpr_tag()
{
  const qreal dpr = QApplication::primaryScreen()->devicePixelRatio();
  return QString::number(dpr, 'f', 2).replace('.', '_');
}

void test_real_dpi()
{
  section("real Qt DPI in this process");

  QScreen    *screen = QApplication::primaryScreen();
  const qreal dpr = screen->devicePixelRatio();
  const QRect available = screen->availableGeometry();

  std::cout << "  ..   QT_SCALE_FACTOR='"
            << qgetenv("QT_SCALE_FACTOR").toStdString() << "' devicePixelRatio " << dpr
            << " available logical geometry " << available.width() << "x"
            << available.height() << "\n";

  check(dpr > 0.0, "the screen reports a usable device pixel ratio");

  // untouched defaults: nothing in this test multiplies a metric, so if a
  // logical size comes out different at a different QT_SCALE_FACTOR then
  // something in the widget is scaling on top of Qt
  const NodePaletteStyle style;

  // the nodes go on the category root, not in a subcategory, so the *top-level*
  // flyout is the one that has to be taller than the screen and scroll
  std::map<std::string, std::string> inventory = sample_inventory();
  for (int i = 0; i < 160; ++i)
    inventory["Node" + std::to_string(i)] = "Primitive";

  NodePaletteSidebar sidebar(inventory, sample_colors(), style);
  sidebar.resize(sidebar.minimumWidth(), std::min(600, available.height()));
  sidebar.show();
  QApplication::processEvents();

  check(sidebar.button_at(0)->sizeHint().height() == style.button_height,
        "a rail button's logical height is the style's, whatever the scale");
  check(sidebar.get_style().button_height == style.button_height,
        "the style itself was not rewritten by being applied");
  check(sidebar.get_style().flyout_row_height == style.flyout_row_height,
        "nor was the flyout row height");

  // every flyout, opened through the real popup path at the real scale
  bool all_on_screen = true;
  bool none_too_tall = true;
  int  scrolled = 0;

  for (int i = 0; i < sidebar.category_count(); ++i)
  {
    sidebar.open_category(i);
    QApplication::processEvents();

    QMenu      *menu = sidebar.menu_at(i);
    const QRect geom = menu->geometry();

    const bool inside = geom.left() >= available.left() &&
                        geom.right() <= available.right() &&
                        geom.top() >= available.top() &&
                        geom.bottom() <= available.bottom();

    if (!inside)
      std::cout << "  ..   flyout " << sidebar.category_names().at(i).toStdString()
                << " geometry " << geom.x() << "," << geom.y() << " "
                << geom.width() << "x" << geom.height() << " outside " << available.x()
                << "," << available.y() << " " << available.width() << "x"
                << available.height() << "\n";

    all_on_screen &= inside;
    none_too_tall &= geom.height() <= available.height();

    if (menu->sizeHint().height() > available.height())
      ++scrolled;
  }

  sidebar.close_flyout();

  check(all_on_screen, "every category flyout opens inside the available screen area");
  check(none_too_tall, "no flyout is taller than the screen");
  std::cout << "  ..   " << scrolled << " of " << sidebar.category_count()
            << " flyouts wanted more height than the screen has\n";
  check(scrolled > 0,
        "the 160-node category really does overflow the screen, so the clamp above "
        "was exercised rather than trivially true");

  sidebar.hide();
}

void render_reference_images()
{
  section("sidebar: rendered references");

  auto render = [](NodePaletteSidebar &sidebar, const QString &fname)
  {
    sidebar.resize(sidebar.minimumWidth() + 40, 560);
    sidebar.show();
    QApplication::processEvents();

    QImage image(sidebar.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor("#2b2b2b"));
    QPainter painter(&image);
    sidebar.render(&painter);
    painter.end();

    const bool saved = image.save(fname);
    check(saved, ("rendered " + fname).toStdString());
    sidebar.hide();
  };

  // Named by this process's actual device pixel ratio, and always with the
  // untouched default style: running the binary under several QT_SCALE_FACTOR
  // values then produces a real set of scaled renders. Faking the scale by
  // multiplying the metrics here would only ever show what the metrics do, not
  // what Qt does with them.
  const QString tag = dpr_tag();

  {
    NodePaletteSidebar sidebar(sample_inventory(), sample_colors(), NodePaletteStyle{});
    render(sidebar, "ui-palette-rail-dpr" + tag + ".png");
  }

  {
    // the flyout itself, so the neutral surface and the restrained category
    // accent can be looked at rather than described
    NodePaletteSidebar sidebar(sample_inventory(), sample_colors(), NodePaletteStyle{});
    sidebar.resize(sidebar.minimumWidth() + 40, 560);
    sidebar.show();
    QApplication::processEvents();

    const int erosion = sidebar.category_names().indexOf("Erosion");
    sidebar.open_category(erosion);
    QApplication::processEvents();

    QMenu *menu = sidebar.menu_at(erosion);
    QTest::qWait(120);
    std::cout << "  ..   flyout logical size " << menu->width() << "x" << menu->height()
              << "\n";
    check(menu->grab().save("ui-palette-flyout-dpr" + tag + ".png"),
          "rendered ui-palette-flyout");

    QMenu *sub = nullptr;
    for (QAction *action : menu->actions())
      if (action->menu())
      {
        sub = action->menu();
        break;
      }

    if (sub)
    {
      sub->popup(menu->mapToGlobal(QPoint(menu->width(), 0)));
      QTest::qWait(120);
      check(sub->grab().save("ui-palette-flyout-nodes-dpr" + tag + ".png"),
            "rendered ui-palette-flyout-nodes");
      sub->close();
    }

    sidebar.close_flyout();
    sidebar.hide();
  }

  {
    // the settings dialog at this scale, capped to the real screen
    ScrollableDialog dialog(make_settings_stub(40), "Application Settings");
    dialog.show();
    QApplication::processEvents();
    QTest::qWait(80);
    check(dialog.grab().save("ui-settings-dialog-dpr" + tag + ".png"),
          "rendered ui-settings-dialog");
    dialog.hide();
  }
}

int main(int argc, char *argv[])
{
  QApplication app(argc, argv);

  // the sidebar's activation signals carry std::string, like the library tree's
  qRegisterMetaType<std::string>("std::string");

  test_scale_sanitizing();
  test_scale_never_compounds();
  test_scale_malformed_config();
  test_scale_environment_override();
  test_executable_path();

  test_settings_dialog();

  test_category_completeness();
  test_node_creation_signal();
  test_search();
  test_flyout_geometry();
  test_animation_toggle();
  test_restyle();
  test_real_dpi();
  render_reference_images();

  std::cout << "\n" << (g_failures == 0 ? "ALL PASSED" : "FAILURES") << ": "
            << g_failures << " failing check(s)\n";

  return g_failures == 0 ? 0 : 1;
}
