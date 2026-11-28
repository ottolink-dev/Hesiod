/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <utility>

#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QScreen>
#include <QSet>
#include <QShortcut>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/controls/control_tones.hpp"
#include "hesiod/gui/widgets/controls/toggle_switch.hpp"
#include "hesiod/gui/widgets/dialogs/example_selector_dialog.hpp"
#include "hesiod/gui/widgets/dialogs/start_screen/example_tile.hpp"
#include "hesiod/gui/widgets/dialogs/start_screen/glide_scroll_area.hpp"
#include "hesiod/gui/widgets/dialogs/start_screen/recent_row.hpp"
#include "hesiod/gui/widgets/dialogs/start_screen/start_action.hpp"
#include "hesiod/gui/widgets/dialogs/start_screen/start_screen_style.hpp"
#include "hesiod/gui/widgets/shell/window_chrome.hpp"
#include "hesiod/gui/widgets/utils/gui_utils.hpp"
#include "hesiod/logger.hpp"
#include "meta_qt/ui/theme.hpp"

namespace hesiod
{

namespace ss = start_screen;

namespace
{

QString pretty_project_name(const QFileInfo &info)
{
  QString name = info.completeBaseName();
  if (name.startsWith('_'))
    name.remove(0, 1);
  name.replace('_', ' ');

  bool capitalize = true;
  for (QChar &character : name)
  {
    if (capitalize && character.isLetter())
      character = character.toUpper();
    capitalize = character.isSpace();
  }
  return name;
}

QString preview_for_project(const QFileInfo &info)
{
  const QString     stem = info.absolutePath() + '/' + info.completeBaseName();
  const QStringList candidates = {stem + "_preview.jpg",
                                  stem + "_preview.png",
                                  stem + ".jpg",
                                  stem + ".png"};
  for (const QString &candidate : candidates)
    if (QFileInfo::exists(candidate))
      return candidate;
  return {};
}

// a label in one of the start screen's inks, on whatever is behind it
QLabel *make_label(const QString &text,
                   const QFont   &font,
                   const QColor  &ink,
                   QWidget       *parent)
{
  auto *label = new QLabel(text, parent);
  label->setFont(font);
  label->setStyleSheet(
      QStringLiteral("color: %1; background: transparent;").arg(ink.name()));
  return label;
}

} // namespace

ExampleSelectorDialog::ExampleSelectorDialog(const QString &examples_path,
                                             QWidget       *parent)
    : QDialog(parent), examples_path(examples_path)
{
  Logger::log()->trace("ExampleSelectorDialog::ExampleSelectorDialog");

  this->setWindowTitle("Hesiod");
  this->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
  this->setModal(true);
  this->setAttribute(Qt::WA_TranslucentBackground);
  this->setFont(meta::qt::ui_font(13));

  // the lockup is drawn in light grays, made for a dark page; on a light
  // theme the name is set in type instead
  const auto &colors = HSD_CTX.app_settings.colors;
  if (colors.text_primary.lightnessF() > colors.bg_primary.lightnessF())
    this->lockup.load("data/hesiod_lockup.png");

  // --- size: generous, but never the whole screen
  QScreen    *screen = parent ? parent->screen() : QGuiApplication::primaryScreen();
  const QRect available = screen ? screen->availableGeometry() : QRect(0, 0, 1280, 800);
  const int   card_w = std::clamp(
      qRound(available.width() * ss::card_width_ratio),
      std::min(ss::card_min_width, available.width() - ss::card_screen_margin),
      std::min(ss::card_max_width, available.width() - ss::card_screen_margin));
  const int card_h = std::clamp(
      qRound(available.height() * ss::card_height_ratio),
      std::min(ss::card_min_height, available.height() - ss::card_screen_margin),
      std::min(ss::card_max_height, available.height() - ss::card_screen_margin));
  this->setFixedSize(card_w + 2 * ss::shadow, card_h + 2 * ss::shadow);
  this->move(available.center() - this->rect().center());

  // --- the two sides
  auto *root = new QHBoxLayout(this);
  root->setContentsMargins(ss::shadow, ss::shadow, ss::shadow, ss::shadow);
  root->setSpacing(0);

  auto *rail = new QWidget(this);
  rail->setFixedWidth(ss::rail_width);
  rail->setAttribute(Qt::WA_NoSystemBackground);
  this->build_rail(rail);
  root->addWidget(rail);

  auto *main = new QWidget(this);
  main->setAttribute(Qt::WA_NoSystemBackground);
  this->build_examples(main);
  root->addWidget(main, 1);

  // --- close, in the corner of the card
  this->close_button = new WindowButton(WindowButton::Kind::Close, this);
  this->close_button->setFixedSize(ss::close_width, ss::close_height);
  this->close_button->setToolTip(QStringLiteral("Close"));
  this->close_button->setAccessibleName(QStringLiteral("Close"));
  QObject::connect(this->close_button,
                   &QAbstractButton::clicked,
                   this,
                   &ExampleSelectorDialog::on_reject);

  // --- the keys that do what the two actions do
  auto *new_keys = new QShortcut(QKeySequence::New, this);
  QObject::connect(new_keys,
                   &QShortcut::activated,
                   this,
                   &ExampleSelectorDialog::start_new_project);
  auto *open_keys = new QShortcut(QKeySequence::Open, this);
  QObject::connect(open_keys,
                   &QShortcut::activated,
                   this,
                   &ExampleSelectorDialog::open_project);

  this->rebuild_recent_projects();
}

ExampleSelectorDialog::~ExampleSelectorDialog() = default;

void ExampleSelectorDialog::build_rail(QWidget *host)
{
  const ControlTones t = control_tones();

  auto *layout = new QVBoxLayout(host);
  layout->setContentsMargins(ss::rail_inset, 0, ss::rail_inset, ss::rail_bottom);
  layout->setSpacing(4);

  // the name of the application is painted by the dialog itself, above this
  layout->addSpacing(ss::rail_head);

  auto *new_action = new StartAction(
      QStringLiteral("New project"),
      QKeySequence(QKeySequence::New).toString(QKeySequence::NativeText),
      StartAction::Glyph::New,
      true,
      host);
  QObject::connect(new_action,
                   &QAbstractButton::clicked,
                   this,
                   &ExampleSelectorDialog::start_new_project);
  layout->addWidget(new_action);
  this->first_action = new_action;
  this->action_arrivals.push_back([new_action](int delay)
                                  { new_action->arrival.arrive(delay); });

  auto *open_action = new StartAction(
      QStringLiteral("Open project…"),
      QKeySequence(QKeySequence::Open).toString(QKeySequence::NativeText),
      StartAction::Glyph::Open,
      false,
      host);
  QObject::connect(open_action,
                   &QAbstractButton::clicked,
                   this,
                   &ExampleSelectorDialog::open_project);
  layout->addWidget(open_action);
  this->action_arrivals.push_back([open_action](int delay)
                                  { open_action->arrival.arrive(delay); });

  layout->addSpacing(ss::rail_section_gap);

  auto *recent_caption = make_label(QStringLiteral("RECENT"),
                                    meta::qt::ui_font(11, true, 0.7),
                                    t.ink_faint,
                                    host);
  recent_caption->setContentsMargins(ss::caption_inset, 0, 0, 0);
  layout->addWidget(recent_caption);
  layout->addSpacing(4);

  this->recent_host = new QWidget(host);
  this->recent_host->setAttribute(Qt::WA_NoSystemBackground);
  this->recent_layout = new QVBoxLayout(this->recent_host);
  this->recent_layout->setContentsMargins(0, 0, 0, 0);
  this->recent_layout->setSpacing(2);
  layout->addWidget(this->recent_host);

  layout->addStretch(1);

  // --- foot of the rail: whether this screen shows at start, and the version
  auto *foot = new QHBoxLayout();
  foot->setContentsMargins(11, 0, 9, 0);
  foot->setSpacing(10);

  this->startup_toggle = new ToggleSwitch(host);
  this->startup_toggle->setChecked(
      HSD_CTX.app_settings.interface.enable_example_selector_at_startup);
  this->startup_toggle->sync();
  this->startup_toggle->setAccessibleName(QStringLiteral("Show at startup"));
  foot->addWidget(this->startup_toggle);

  foot->addWidget(make_label(QStringLiteral("Show at startup"),
                             meta::qt::ui_font(12),
                             t.ink_dim,
                             host));
  foot->addStretch(1);

  foot->addWidget(make_label(QStringLiteral("v%1.%2.%3")
                                 .arg(HESIOD_VERSION_MAJOR)
                                 .arg(HESIOD_VERSION_MINOR)
                                 .arg(HESIOD_VERSION_PATCH),
                             meta::qt::mono_font(11),
                             t.ink_faint,
                             host));
  layout->addLayout(foot);

  QObject::connect(
      this->startup_toggle,
      &QAbstractButton::toggled,
      this,
      [](bool checked)
      {
        HSD_CTX.app_settings.interface.enable_example_selector_at_startup = checked;
        HSD_CTX.save_settings();
      });
}

void ExampleSelectorDialog::build_examples(QWidget *host)
{
  const ControlTones t = control_tones();

  QDir              dir(this->examples_path);
  const QStringList files = dir.entryList({"*.hsd"},
                                          QDir::Files | QDir::Readable,
                                          QDir::Name | QDir::IgnoreCase);

  // the picture that leads: the most striking of the set when it is there
  QStringList ordered;
  for (const QString &lead : {QStringLiteral("island.hsd"),
                              QStringLiteral("yukon_valley.hsd"),
                              QStringLiteral("facing_strata.hsd")})
    if (files.contains(lead, Qt::CaseInsensitive))
    {
      ordered << lead;
      break;
    }
  for (const QString &file : files)
    if (!ordered.contains(file, Qt::CaseInsensitive))
      ordered << file;

  // The "default" project's picture is a screenshot of the application, the
  // least inviting of the set: it goes near the end, and the last place is
  // kept for a landscape, which may be shown wide (see layout_tiles).
  for (int i = 0; i < ordered.size(); ++i)
    if (ordered[i].startsWith('_') && ordered.size() >= 3)
    {
      ordered.move(i, ordered.size() - 2);
      break;
    }

  auto *layout = new QVBoxLayout(host);
  layout->setContentsMargins(ss::examples_left, ss::examples_top, ss::examples_right, 0);
  layout->setSpacing(0);

  layout->addWidget(make_label(QStringLiteral("Start from an example"),
                               meta::qt::ui_font(19, true),
                               t.ink,
                               host));
  layout->addSpacing(4);

  auto *lede = make_label(
      ordered.isEmpty()
          ? QStringLiteral("No example projects were found in %1.")
                .arg(QDir::toNativeSeparators(dir.absolutePath()))
          : QStringLiteral("Finished terrains to open, pull apart and make your own."),
      meta::qt::ui_font(13),
      t.ink_dim,
      host);
  lede->setWordWrap(true);
  layout->addWidget(lede);
  layout->addSpacing(ss::examples_lede_gap);

  this->scroll_area = new GlideScrollArea(host);
  this->scroll_area->setFrameShape(QFrame::NoFrame);
  this->scroll_area->setWidgetResizable(true);
  this->scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  this->scroll_area->setStyleSheet(
      QStringLiteral("QScrollArea { background: transparent; border: none; }"));
  this->scroll_area->viewport()->setAutoFillBackground(false);

  this->tiles_host = new QWidget(this->scroll_area);
  this->tiles_host->setAttribute(Qt::WA_NoSystemBackground);
  this->tiles_host->setStyleSheet(QStringLiteral("background: transparent;"));

  for (int index = 0; index < ordered.size(); ++index)
  {
    const QFileInfo info(dir.filePath(ordered[index]));
    const QString   path = info.absoluteFilePath();
    auto           *tile = new ExampleTile(pretty_project_name(info),
                                 preview_for_project(info),
                                 index == 0 && ordered.size() >= 3,
                                 this->tiles_host);
    QObject::connect(tile,
                     &QAbstractButton::clicked,
                     this,
                     [this, path]() { this->accept_path(path, false); });
    this->tiles.push_back(tile);
  }

  this->scroll_area->setWidget(this->tiles_host);
  layout->addWidget(this->scroll_area, 1);
}

// The pictures are laid out by hand: three columns of 16:9 cells, the first
// picture taking two columns and two rows. A grid layout cannot keep cells at
// a ratio, and the ratio is what makes the page look composed.
void ExampleSelectorDialog::layout_tiles()
{
  if (!this->scroll_area || !this->tiles_host || this->tiles.isEmpty())
    return;

  const int   gap = ss::tile_gap;
  const int   columns = ss::tile_columns;
  const int   width = this->scroll_area->viewport()->width() - ss::tile_right_room;
  const qreal cell_w = (width - (columns - 1) * gap) / qreal(columns);
  const qreal cell_h = cell_w * 9.0 / 16.0;

  auto place = [&](ExampleTile *tile, int column, int row, int span_x, int span_y)
  {
    if (!tile)
      return;
    const int x = qRound(column * (cell_w + gap));
    const int y = qRound(row * (cell_h + gap));
    const int w = qRound((column + span_x) * (cell_w + gap) - gap) - x;
    const int h = qRound((row + span_y) * (cell_h + gap) - gap) - y;
    tile->setGeometry(x, y, w, h);
  };

  int        column = 0, row = 0, index = 0;
  const bool lead = this->tiles.size() >= 3;
  if (lead)
  {
    place(this->tiles[0], 0, 0, 2, 2);
    place(this->tiles[1], 2, 0, 1, 1);
    place(this->tiles[2], 2, 1, 1, 1);
    index = 3;
    row = 2;
  }
  for (; index < this->tiles.size(); ++index)
  {
    // a last row that would stop one cell short is closed by its last
    // picture, shown two cells wide
    const bool closes_row = index == this->tiles.size() - 1 && column == columns - 2;
    place(this->tiles[index], column, row, closes_row ? 2 : 1, 1);
    column += closes_row ? 2 : 1;
    if (column >= columns)
    {
      column = 0;
      ++row;
    }
  }

  const int rows = row + (column > 0 ? 1 : 0);
  const int height = qRound(rows * (cell_h + gap) - gap) + ss::tile_bottom_air;
  this->tiles_host->setFixedHeight(height);
}

void ExampleSelectorDialog::rebuild_recent_projects()
{
  if (!this->recent_layout || !this->recent_host)
    return;

  while (QLayoutItem *item = this->recent_layout->takeAt(0))
  {
    if (QWidget *widget = item->widget())
      widget->deleteLater();
    delete item;
  }

  this->recent_arrivals.clear();

  QSet<QString> seen;
  int           shown = 0;

  for (const std::string &raw_path : HSD_CTX.app_settings.global.recent_files)
  {
    const QFileInfo info(QString::fromStdString(raw_path));
    if (!info.exists() || !info.isFile() ||
        info.suffix().compare("hsd", Qt::CaseInsensitive) != 0)
      continue;

    const QString normalized = QDir::cleanPath(info.absoluteFilePath()).toLower();
    if (seen.contains(normalized))
      continue;
    seen.insert(normalized);

    const QString path = info.absoluteFilePath();
    auto         *row = new RecentRow(pretty_project_name(info),
                              info.lastModified().toString("d MMM"),
                              QDir::toNativeSeparators(info.absolutePath()),
                              this->recent_host);
    row->setToolTip(QDir::toNativeSeparators(path));
    QObject::connect(row,
                     &QAbstractButton::clicked,
                     this,
                     [this, path]() { this->accept_path(path, true); });
    this->recent_layout->addWidget(row);
    this->recent_arrivals.push_back([row](int delay) { row->arrival.arrive(delay); });

    if (++shown >= ss::recent_max)
      break;
  }

  if (shown == 0)
  {
    auto *empty = make_label(
        QStringLiteral("Projects you open or save will be listed here."),
        meta::qt::ui_font(12),
        control_tones().ink_faint,
        this->recent_host);
    empty->setWordWrap(true);
    empty->setContentsMargins(ss::caption_inset, 4, ss::caption_inset, 4);
    this->recent_layout->addWidget(empty);
  }
}

// The entrance. The card itself is brought in by the application's dialog
// motion; on top of that the parts arrive a beat apart: the name, the two
// actions, the recent projects, then the pictures, the large one first.
void ExampleSelectorDialog::start_reveal()
{
  if (std::exchange(this->revealed, true))
    return;

  if (motion::enabled())
  {
    this->brand_t.set(0.0);
    QTimer::singleShot(
        motion::start_lead,
        this,
        [this]() { this->brand_t.to(1.0, motion::start_brand_in, motion::enter()); });
  }
  else
    this->brand_t.set(1.0);

  int delay = motion::start_lead + motion::start_actions_after;
  for (const auto &arrive : this->action_arrivals)
  {
    arrive(delay);
    delay += motion::start_action_step;
  }
  delay += motion::start_recent_after;
  for (const auto &arrive : this->recent_arrivals)
  {
    arrive(delay);
    delay += motion::start_recent_step;
  }

  int tile_delay = motion::start_lead + motion::start_tiles_after;
  for (ExampleTile *tile : this->tiles)
  {
    if (tile)
      tile->arrival.arrive(tile_delay);
    tile_delay += motion::start_tile_step;
  }
}

void ExampleSelectorDialog::showEvent(QShowEvent *event)
{
  QDialog::showEvent(event);
  this->layout_tiles();
  this->start_reveal();
  if (this->first_action)
    this->first_action->setFocus(Qt::OtherFocusReason);

  // the viewport has its final width only once the dialog is laid out
  QTimer::singleShot(0, this, [this]() { this->layout_tiles(); });
}

void ExampleSelectorDialog::resizeEvent(QResizeEvent *event)
{
  QDialog::resizeEvent(event);
  if (this->close_button)
    this->close_button->move(this->width() - ss::shadow - this->close_button->width() -
                                 ss::close_inset,
                             ss::shadow + ss::close_inset);
  this->layout_tiles();
}

void ExampleSelectorDialog::paintEvent(QPaintEvent *)
{
  const ControlTones t = control_tones();

  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  const QRectF card = QRectF(this->rect())
                          .adjusted(ss::shadow, ss::shadow, -ss::shadow, -ss::shadow);

  // soft shadow: stacked rounded shapes fading out, a little lower than the card
  p.setPen(Qt::NoPen);
  for (int i = ss::shadow; i > 0; i -= 2)
  {
    const qreal s = 1.0 - qreal(i) / ss::shadow;
    p.setBrush(QColor(0, 0, 0, qRound(ss::shadow_alpha * s * s)));
    p.drawRoundedRect(card.adjusted(-i, -i + ss::shadow_drop, i, i + ss::shadow_drop),
                      ss::card_radius + i,
                      ss::card_radius + i);
  }

  QPainterPath shape;
  shape.addRoundedRect(card.adjusted(0.5, 0.5, -0.5, -0.5),
                       ss::card_radius,
                       ss::card_radius);
  p.fillPath(shape, t.card);

  // the rail: the same card, a step darker on its left
  p.save();
  p.setClipPath(shape);
  p.fillRect(QRectF(card.left(), card.top(), ss::rail_width, card.height()), t.content);
  p.restore();

  p.setPen(QPen(t.border, 1));
  p.setBrush(Qt::NoBrush);
  p.drawPath(shape);

  this->paint_brand(p, card);
}

// the name of the application, at the head of the rail
void ExampleSelectorDialog::paint_brand(QPainter &p, const QRectF &card)
{
  const qreal a = std::clamp(this->brand_t.value(), 0.0, 1.0);
  if (a <= 0.001)
    return;

  const ControlTones t = control_tones();

  p.save();
  p.setOpacity(a);
  p.translate(0.0, ss::brand_rise * (1.0 - a));

  const qreal left = card.left() + ss::brand_left;
  const qreal top = card.top() + ss::brand_top;
  const qreal room = ss::rail_width - 2.0 * ss::brand_left;

  if (!this->lockup.isNull())
  {
    const qreal h = ss::brand_height;
    const qreal w = h * this->lockup.width() / qreal(this->lockup.height());
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.drawPixmap(QRectF(left, top, w, h), this->lockup, QRectF(this->lockup.rect()));
  }
  else
  {
    p.setFont(meta::qt::ui_font(22, true));
    p.setPen(t.ink);
    p.drawText(QRectF(left, top, room, ss::brand_height),
               Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("Hesiod"));
  }

  p.setFont(meta::qt::ui_font(12));
  p.setPen(t.ink_faint);
  p.drawText(QRectF(left + 1.0, top + ss::brand_tagline_gap, room, 18.0),
             Qt::AlignLeft | Qt::AlignVCenter,
             QStringLiteral("Procedural terrain design"));
  p.restore();
}

void ExampleSelectorDialog::keyPressEvent(QKeyEvent *event)
{
  if (event->key() == Qt::Key_Escape)
  {
    this->on_reject();
    return;
  }
  QDialog::keyPressEvent(event);
}

// the card has no title bar: it is moved by any part that is not a control
void ExampleSelectorDialog::mousePressEvent(QMouseEvent *event)
{
  if (event->button() == Qt::LeftButton)
  {
    this->dragging = true;
    this->drag_offset = event->globalPosition().toPoint() -
                        this->frameGeometry().topLeft();
    event->accept();
    return;
  }
  QDialog::mousePressEvent(event);
}

void ExampleSelectorDialog::mouseMoveEvent(QMouseEvent *event)
{
  if (this->dragging && (event->buttons() & Qt::LeftButton))
  {
    this->move(event->globalPosition().toPoint() - this->drag_offset);
    event->accept();
    return;
  }
  QDialog::mouseMoveEvent(event);
}

void ExampleSelectorDialog::mouseReleaseEvent(QMouseEvent *event)
{
  this->dragging = false;
  QDialog::mouseReleaseEvent(event);
}

void ExampleSelectorDialog::accept_path(const QString &path, bool project)
{
  if (path.isEmpty())
    return;
  this->selected_filename = path;
  this->selected_project = project;
  this->result = Outcome::OpenFile;
  this->accept();
}

void ExampleSelectorDialog::on_reject()
{
  this->selected_filename.clear();
  this->selected_project = false;
  this->result = Outcome::Closed;
  this->reject();
}

void ExampleSelectorDialog::open_project()
{
  const QString path = QFileDialog::getOpenFileName(
      this,
      QStringLiteral("Open Hesiod Project"),
      QDir::homePath(),
      QStringLiteral("Hesiod Project (*.hsd)"));
  if (!path.isEmpty())
    this->accept_path(path, true);
}

void ExampleSelectorDialog::start_new_project()
{
  this->selected_filename.clear();
  this->selected_project = false;
  this->result = Outcome::NewProject;
  this->accept();
}

ExampleSelectorDialog::Outcome ExampleSelectorDialog::outcome() const
{
  return this->result;
}

QString ExampleSelectorDialog::selected_file() const { return this->selected_filename; }

bool ExampleSelectorDialog::selected_is_project() const { return this->selected_project; }

} // namespace hesiod
