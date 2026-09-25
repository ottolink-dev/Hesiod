/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <chrono>
#include <climits>
#include <cmath>

#include <QAbstractButton>
#include <QButtonGroup>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QScreen>
#include <QScrollArea>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariantAnimation>

#include "meta_qt/ui/theme.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/app/ui_scale.hpp"
#include "hesiod/gui/widgets/app_settings_window.hpp"
#include "hesiod/gui/widgets/color_picker_dialog.hpp"
#include "hesiod/gui/widgets/gui_utils.hpp"
#include "hesiod/gui/widgets/message_dialog.hpp"
#include "hesiod/gui/widgets/node_palette_sidebar.hpp"
#include "hesiod/gui/widgets/viewers/viewport_controls.hpp"
#include "hesiod/gui/widgets/window_chrome.hpp"
#include "hesiod/logger.hpp"

// accessor for one field of AppSettings, usable on the live settings and on a
// default-constructed copy alike
#define HSD_SETTING(type, path)                                                          \
  [](AppSettings &s) -> type & { return s.path; }

namespace hesiod
{

// =====================================
// draft bookkeeping
// =====================================

struct AppSettingsWindow::Binding
{
  std::function<bool()> dirty;        // draft differs from the running value
  std::function<void()> commit;       // running value = draft
  std::function<void()> revert;       // draft = running value, refresh control
  std::function<void()> load_default; // draft = default value, refresh control
  unsigned              effects = NoEffect;
  bool                  restart = false;
  QString               label;
};

struct AppSettingsWindow::Row
{
  QWidget                 *box = nullptr;     // divider + row
  QWidget                 *divider = nullptr; // hairline above the row
  QWidget                 *row = nullptr;
  QLabel                  *modified = nullptr;
  QWidget                 *focus = nullptr;
  Card                    *card = nullptr;
  int                      page = 0;
  std::shared_ptr<Binding> binding;

  // lower-cased search fields
  QString label, keywords, description, context;
};

struct AppSettingsWindow::Card
{
  QFrame            *frame = nullptr;
  QLabel            *heading = nullptr;
  std::vector<Row *> rows;
};

namespace
{

constexpr int kRadius = 10;    // dialog corners
constexpr int kSidebarW = 220; // section list width
constexpr int kHeaderH = 52;   // draggable band at the top
constexpr int kControlW = 150; // right-hand control column

// the applied restart-only settings of this session; the banner outlives the
// dialog so reopening it still says a restart is due
QStringList g_restart_pending;

QColor mix(const QColor &from, const QColor &to, qreal amount)
{
  amount = std::clamp(amount, 0.0, 1.0);
  return QColor::fromRgbF(from.redF() + (to.redF() - from.redF()) * amount,
                          from.greenF() + (to.greenF() - from.greenF()) * amount,
                          from.blueF() + (to.blueF() - from.blueF()) * amount);
}

AppSettings &defaults()
{
  static AppSettings instance;
  return instance;
}

struct Tones
{
  QColor sidebar, content, card, border, field, field_hover, ink, ink_dim, ink_faint,
      accent, accent_soft;
};

Tones tones()
{
  const auto &c = HSD_CTX.app_settings.colors;
  Tones       t;
  t.sidebar = c.bg_deep;
  t.content = mix(c.bg_deep, c.bg_primary, 0.40);
  t.card = c.bg_primary;
  t.border = mix(c.bg_primary, c.border, 0.38);
  t.field = mix(c.bg_deep, c.bg_primary, 0.55);
  t.field_hover = mix(c.bg_primary, c.border, 0.25);
  t.ink = c.text_primary;
  t.ink_dim = mix(c.bg_primary, c.text_primary, 0.62);
  t.ink_faint = mix(c.bg_primary, c.text_primary, 0.42);
  t.accent = c.accent;
  t.accent_soft = mix(c.bg_primary, c.accent, 0.30);
  return t;
}

void repolish(QWidget *widget)
{
  widget->style()->unpolish(widget);
  widget->style()->polish(widget);
  widget->update();
}

// ---------------------------------------------------------------------------
// fuzzy matching
// ---------------------------------------------------------------------------

// optimal string alignment distance: edits, with adjacent swaps counting as one
int edit_distance(const QString &a, const QString &b)
{
  const int                     n = int(a.size());
  const int                     m = int(b.size());
  std::vector<std::vector<int>> d(n + 1, std::vector<int>(m + 1));
  for (int i = 0; i <= n; ++i)
    d[i][0] = i;
  for (int j = 0; j <= m; ++j)
    d[0][j] = j;

  for (int i = 1; i <= n; ++i)
    for (int j = 1; j <= m; ++j)
    {
      const int cost = a[i - 1] == b[j - 1] ? 0 : 1;
      d[i][j] = std::min({d[i - 1][j] + 1, d[i][j - 1] + 1, d[i - 1][j - 1] + cost});
      if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1])
        d[i][j] = std::min(d[i][j], d[i - 2][j - 2] + 1);
    }
  return d[n][m];
}

bool word_start(const QString &text, qsizetype i)
{
  return i == 0 || !text[i - 1].isLetterOrNumber();
}

// Score of one query token against one (lower-cased) field; 0 = no match.
// Substrings win, then abbreviations ("ui scl" -> "interface scale" does not,
// but "intsc" does), then typos ("scael", "animtions").
int token_score(const QString &token, const QString &text, bool allow_subsequence)
{
  if (token.isEmpty() || text.isEmpty())
    return 0;

  // substring, best at a word start and early in the text
  const qsizetype at = text.indexOf(token);
  if (at >= 0)
    return 100 + (word_start(text, at) ? 40 : 0) - int(std::min<qsizetype>(at, 30));

  // abbreviation: every character in order, rewarding word starts and runs
  if (allow_subsequence && token.size() >= 2)
  {
    int       score = 0, streak = 0;
    qsizetype ti = 0, first = -1, last = -1;
    for (qsizetype i = 0; i < text.size() && ti < token.size(); ++i)
    {
      if (text[i] == token[ti])
      {
        if (first < 0)
          first = i;
        last = i;
        score += 2 + (word_start(text, i) ? 6 : 0) + 3 * streak;
        ++streak;
        ++ti;
      }
      else
        streak = 0;
    }

    // all found, and not scattered over the whole field
    if (ti == token.size() && (last - first) <= 4 * token.size())
      return std::clamp(score, 10, 90);
  }

  // typo tolerance against each word (and each word's prefix, for words the
  // user has not finished typing)
  if (token.size() >= 4)
  {
    static const QRegularExpression separators("[^\\p{L}\\p{N}]+");
    int                             best = INT_MAX;
    for (const QString &word : text.split(separators, Qt::SkipEmptyParts))
    {
      best = std::min(best, edit_distance(token, word));
      if (word.size() > token.size())
        best = std::min(best, edit_distance(token, word.left(token.size())));
    }

    const int allowed = token.size() >= 7 ? 2 : 1;
    if (best <= allowed)
      return 60 - 20 * best;
  }

  return 0;
}

// ---------------------------------------------------------------------------
// ToggleSwitch: pill switch replacing the bare checkbox
// ---------------------------------------------------------------------------

class ToggleSwitch final : public QAbstractButton
{
public:
  explicit ToggleSwitch(QWidget *parent = nullptr) : QAbstractButton(parent)
  {
    this->setCheckable(true);
    this->setCursor(Qt::PointingHandCursor);
    this->setFixedSize(38, 22);

    this->animation = new QVariantAnimation(this);
    this->animation->setDuration(140);
    this->animation->setEasingCurve(QEasingCurve::OutCubic);
    QObject::connect(this->animation,
                     &QVariantAnimation::valueChanged,
                     this,
                     [this](const QVariant &value)
                     {
                       this->position = value.toReal();
                       this->update();
                     });

    QObject::connect(this,
                     &QAbstractButton::toggled,
                     this,
                     [this](bool checked)
                     {
                       const qreal target = checked ? 1.0 : 0.0;
                       if (!HSD_CTX.app_settings.interface.enable_ui_animations ||
                           !this->isVisible())
                       {
                         this->position = target;
                         this->update();
                         return;
                       }
                       this->animation->stop();
                       this->animation->setStartValue(this->position);
                       this->animation->setEndValue(target);
                       this->animation->start();
                     });
  }

  // jump to the checked state without animating (after a blocked setChecked)
  void sync()
  {
    this->animation->stop();
    this->position = this->isChecked() ? 1.0 : 0.0;
    this->update();
  }

protected:
  void paintEvent(QPaintEvent *) override
  {
    const Tones t = tones();
    QPainter    p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF track = QRectF(this->rect()).adjusted(1, 1, -1, -1);
    const qreal  r = track.height() / 2.0;

    const QColor off = this->underMouse() ? t.field_hover : t.field;
    QColor       edge = mix(t.border, t.accent, this->position);
    if (this->hasFocus())
      edge = t.accent.lighter(130);
    p.setPen(QPen(edge, 1));
    p.setBrush(mix(off, t.accent, this->position));
    p.drawRoundedRect(track, r, r);

    const qreal  d = track.height() - 6.0;
    const qreal  x = track.left() + 3.0 + (track.width() - 6.0 - d) * this->position;
    const QRectF knob(x, track.top() + 3.0, d, d);
    p.setPen(Qt::NoPen);
    p.setBrush(mix(t.ink_dim, QColor("#ffffff"), this->position));
    p.drawEllipse(knob);
  }

  void enterEvent(QEnterEvent *event) override
  {
    this->update();
    QAbstractButton::enterEvent(event);
  }

  void leaveEvent(QEvent *event) override
  {
    this->update();
    QAbstractButton::leaveEvent(event);
  }

private:
  QVariantAnimation *animation = nullptr;
  qreal              position = 0.0;
};

// ---------------------------------------------------------------------------
// StepButton: the - / + ends of a stepper (painted, so the glyphs are centred)
// ---------------------------------------------------------------------------

class StepButton final : public QAbstractButton
{
public:
  StepButton(bool plus, QWidget *parent) : QAbstractButton(parent), plus(plus)
  {
    this->setFixedSize(26, 28);
    this->setAutoRepeat(true);
    this->setAutoRepeatDelay(350);
    this->setAutoRepeatInterval(60);
    this->setFocusPolicy(Qt::NoFocus);
    this->setCursor(Qt::PointingHandCursor);
    this->setToolTip(plus ? "Increase" : "Decrease");
  }

protected:
  void paintEvent(QPaintEvent *) override
  {
    const Tones t = tones();
    QPainter    p(this);
    p.setRenderHint(QPainter::Antialiasing);

    if (this->underMouse() && this->isEnabled())
    {
      p.setPen(Qt::NoPen);
      p.setBrush(this->isDown() ? t.border : t.field_hover);
      p.drawRoundedRect(QRectF(this->rect()).adjusted(2, 2, -2, -2), 5, 5);
    }

    QPen pen(this->isEnabled() ? t.ink_dim : t.ink_faint, 1.4);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    const QPointF c(this->width() / 2.0, this->height() / 2.0);
    p.drawLine(c + QPointF(-4.0, 0.0), c + QPointF(4.0, 0.0));
    if (this->plus)
      p.drawLine(c + QPointF(0.0, -4.0), c + QPointF(0.0, 4.0));
  }

  void enterEvent(QEnterEvent *event) override
  {
    this->update();
    QAbstractButton::enterEvent(event);
  }

  void leaveEvent(QEvent *event) override
  {
    this->update();
    QAbstractButton::leaveEvent(event);
  }

private:
  bool plus;
};

// [-] value [+], around a spin box with its own arrows hidden
QWidget *make_stepper(QAbstractSpinBox *spin)
{
  auto *box = new QFrame();
  box->setObjectName("stepper");
  box->setFixedWidth(kControlW);

  auto *layout = new QHBoxLayout(box);
  layout->setContentsMargins(1, 1, 1, 1);
  layout->setSpacing(0);

  spin->setParent(box);
  spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
  spin->setAlignment(Qt::AlignCenter);
  spin->setFrame(false);
  spin->setKeyboardTracking(false);

  auto *minus = new StepButton(false, box);
  auto *plus = new StepButton(true, box);
  QObject::connect(minus, &QAbstractButton::clicked, spin, &QAbstractSpinBox::stepDown);
  QObject::connect(plus, &QAbstractButton::clicked, spin, &QAbstractSpinBox::stepUp);

  layout->addWidget(minus);
  layout->addWidget(spin, 1);
  layout->addWidget(plus);
  return box;
}

// ---------------------------------------------------------------------------
// ColorSwatch
// ---------------------------------------------------------------------------

class ColorSwatch final : public QAbstractButton
{
public:
  explicit ColorSwatch(QWidget *parent = nullptr) : QAbstractButton(parent)
  {
    this->setCursor(Qt::PointingHandCursor);
    this->setFixedSize(kControlW, 30);
  }

  void set_color(const QColor &value)
  {
    this->color = value;
    this->setToolTip(value.name(value.alpha() < 255 ? QColor::HexArgb : QColor::HexRgb));
    this->update();
  }

protected:
  void paintEvent(QPaintEvent *) override
  {
    const Tones t = tones();
    QPainter    p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF frame = QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QColor       edge = this->underMouse() ? t.ink_faint : t.border;
    if (this->hasFocus())
      edge = t.accent;
    p.setPen(QPen(edge, 1));
    p.setBrush(this->underMouse() ? t.field_hover : t.field);
    p.drawRoundedRect(frame, 6, 6);

    const QRectF chip(5.5, 5.5, 34, this->height() - 11.0);
    p.setPen(QPen(mix(this->color, QColor("#000000"), 0.35), 1));
    p.setBrush(this->color);
    p.drawRoundedRect(chip, 4, 4);

    p.setPen(t.ink_dim);
    p.setFont(meta::qt::mono_font(12));
    p.drawText(
        QRectF(chip.right() + 10, 0, this->width() - chip.right() - 14, this->height()),
        Qt::AlignVCenter | Qt::AlignLeft,
        this->color.name().toUpper());
  }

  void enterEvent(QEnterEvent *event) override
  {
    this->update();
    QAbstractButton::enterEvent(event);
  }

  void leaveEvent(QEvent *event) override
  {
    this->update();
    QAbstractButton::leaveEvent(event);
  }

private:
  QColor color;
};

// Binding for a value held in AppSettings, edited through a draft copy.
// `show` pushes a value into the control without it reporting an edit.
template <typename T>
std::shared_ptr<AppSettingsWindow::Binding> make_binding(
    std::function<T &(AppSettings &)>         access,
    std::shared_ptr<T>                        draft,
    std::function<void(const T &)>            show,
    std::function<bool(const T &, const T &)> equal = [](const T &a, const T &b)
    { return a == b; })
{
  auto binding = std::make_shared<AppSettingsWindow::Binding>();
  binding->dirty = [=]() { return !equal(*draft, access(HSD_CTX.app_settings)); };
  binding->commit = [=]() { access(HSD_CTX.app_settings) = *draft; };
  binding->revert = [=]()
  {
    *draft = access(HSD_CTX.app_settings);
    show(*draft);
  };
  binding->load_default = [=]()
  {
    *draft = access(defaults());
    show(*draft);
  };
  return binding;
}

} // namespace

// =====================================
// AppSettingsWindow
// =====================================

AppSettingsWindow::AppSettingsWindow(QWidget *parent) : QDialog(parent)
{
  Logger::log()->trace("AppSettingsWindow::AppSettingsWindow");

  this->setObjectName("hsdSettings");
  this->setWindowTitle("Settings");
  this->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
  this->setAttribute(Qt::WA_TranslucentBackground);
  this->setModal(true);

  // --- skeleton: [ sidebar | header / pages / banner / footer ]

  auto *root = new QHBoxLayout(this);
  root->setContentsMargins(1, 1, 1, 1);
  root->setSpacing(0);

  auto *sidebar = new QWidget(this);
  sidebar->setObjectName("sidebar");
  sidebar->setFixedWidth(kSidebarW);
  auto *side_layout = new QVBoxLayout(sidebar);
  side_layout->setContentsMargins(14, 16, 14, 16);
  side_layout->setSpacing(2);

  {
    auto *identity = new QHBoxLayout();
    identity->setContentsMargins(6, 0, 0, 0);
    identity->setSpacing(10);

    auto *logo = new QLabel(sidebar);
    logo->setFixedSize(22, 22);
    logo->setPixmap(QIcon(QString::fromStdString(HSD_CTX.app_settings.global.icon_path))
                        .pixmap(QSize(22, 22), this->devicePixelRatioF()));
    logo->setAttribute(Qt::WA_TransparentForMouseEvents);
    identity->addWidget(logo);

    auto *title = new QLabel("Settings", sidebar);
    title->setObjectName("sidebarTitle");
    title->setAttribute(Qt::WA_TransparentForMouseEvents);
    identity->addWidget(title, 1);
    side_layout->addLayout(identity);
  }
  side_layout->addSpacing(16);

  // search
  this->search = new QLineEdit(sidebar);
  this->search->setObjectName("settingsSearch");
  this->search->setPlaceholderText("Search settings");
  this->search->setClearButtonEnabled(true);
  this->search->setToolTip("Ctrl+F. Typos and abbreviations are fine. Enter jumps to "
                           "the best match, Esc clears.");
  {
    // magnifier, drawn so it matches the other hairline glyphs
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(4);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(tones().ink_faint, 1.3);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    p.drawEllipse(QRectF(3.0, 3.0, 7.0, 7.0));
    p.drawLine(QPointF(9.1, 9.1), QPointF(12.5, 12.5));
    p.end();
    this->search->addAction(QIcon(pixmap), QLineEdit::LeadingPosition);
  }
  side_layout->addWidget(this->search);
  side_layout->addSpacing(14);

  this->nav_layout = new QVBoxLayout();
  this->nav_layout->setSpacing(2);
  side_layout->addLayout(this->nav_layout);

  this->search_status = new QLabel(sidebar);
  this->search_status->setObjectName("searchStatus");
  this->search_status->setWordWrap(true);
  this->search_status->hide();
  side_layout->addSpacing(8);
  side_layout->addWidget(this->search_status);
  side_layout->addStretch(1);

  auto *version = new QLabel(QString("Hesiod %1.%2.%3")
                                 .arg(HESIOD_VERSION_MAJOR)
                                 .arg(HESIOD_VERSION_MINOR)
                                 .arg(HESIOD_VERSION_PATCH),
                             sidebar);
  version->setObjectName("sidebarVersion");
  version->setContentsMargins(8, 0, 0, 0);
  side_layout->addWidget(version);

  root->addWidget(sidebar);

  auto *main = new QWidget(this);
  main->setObjectName("main");
  auto *main_layout = new QVBoxLayout(main);
  main_layout->setContentsMargins(0, 0, 0, 0);
  main_layout->setSpacing(0);

  // header: only the close button; the empty band drags the dialog
  auto *header = new QWidget(main);
  header->setFixedHeight(kHeaderH - 12);
  auto *header_layout = new QHBoxLayout(header);
  header_layout->setContentsMargins(0, 8, 10, 0);
  header_layout->addStretch(1);
  auto *close_button = new WindowButton(WindowButton::Kind::Close, header);
  close_button->setFixedSize(34, 30);
  close_button->setToolTip("Close (Esc)");
  header_layout->addWidget(close_button, 0, Qt::AlignTop);
  main_layout->addWidget(header);

  this->pages = new QStackedWidget(main);
  this->pages->setObjectName("pages");
  main_layout->addWidget(this->pages, 1);

  // restart banner
  this->restart_banner = new QFrame(main);
  this->restart_banner->setObjectName("restartBanner");
  {
    auto *layout = new QHBoxLayout(this->restart_banner);
    layout->setContentsMargins(14, 9, 14, 9);
    layout->setSpacing(10);
    auto *icon = new QLabel(this->restart_banner);
    icon->setFixedSize(18, 18);
    icon->setPixmap(MessageDialog::badge(MessageDialog::Kind::Restore,
                                         18,
                                         this->devicePixelRatioF()));
    layout->addWidget(icon);
    this->restart_label = new QLabel(this->restart_banner);
    this->restart_label->setObjectName("restartLabel");
    this->restart_label->setWordWrap(true);
    layout->addWidget(this->restart_label, 1);
  }
  auto *banner_wrap = new QHBoxLayout();
  banner_wrap->setContentsMargins(28, 8, 28, 0);
  banner_wrap->addWidget(this->restart_banner);
  main_layout->addLayout(banner_wrap);

  // footer
  auto *footer = new QWidget(main);
  footer->setObjectName("footer");
  auto *footer_layout = new QHBoxLayout(footer);
  footer_layout->setContentsMargins(28, 12, 20, 16);
  footer_layout->setSpacing(8);

  auto *defaults_button = new QPushButton("Restore defaults", footer);
  defaults_button->setObjectName("ghostButton");
  defaults_button->setCursor(Qt::PointingHandCursor);
  defaults_button->setToolTip("Fill in every default value. Nothing changes until you "
                              "press Apply.");
  footer_layout->addWidget(defaults_button);
  footer_layout->addStretch(1);

  this->footer_status = new QLabel(footer);
  this->footer_status->setObjectName("footerStatus");
  footer_layout->addWidget(this->footer_status);
  footer_layout->addSpacing(6);

  this->discard_button = new QPushButton("Discard", footer);
  this->discard_button->setObjectName("secondaryButton");
  this->discard_button->setCursor(Qt::PointingHandCursor);
  this->discard_button->setToolTip("Forget the changes made since the last Apply");
  footer_layout->addWidget(this->discard_button);

  this->apply_button = new QPushButton("Apply", footer);
  this->apply_button->setObjectName("primaryButton");
  this->apply_button->setCursor(Qt::PointingHandCursor);
  this->apply_button->setToolTip("Apply the changes (Ctrl+Enter)");
  footer_layout->addWidget(this->apply_button);
  main_layout->addWidget(footer);

  root->addWidget(main, 1);

  this->nav_group = new QButtonGroup(this);
  this->nav_group->setExclusive(true);
  this->connect(this->nav_group,
                &QButtonGroup::idClicked,
                this->pages,
                &QStackedWidget::setCurrentIndex);

  this->setup_pages();
  this->apply_stylesheet();
  this->update_state();

  this->resize(940, 680);
  this->fit_to_screen();

  // --- connections

  this->connect(close_button, &QAbstractButton::clicked, this, &QDialog::reject);
  this->connect(this->apply_button,
                &QPushButton::clicked,
                this,
                &AppSettingsWindow::apply_changes);
  this->connect(this->discard_button,
                &QPushButton::clicked,
                this,
                &AppSettingsWindow::discard_changes);
  this->connect(defaults_button,
                &QPushButton::clicked,
                this,
                &AppSettingsWindow::load_defaults);

  this->connect(this->search,
                &QLineEdit::textChanged,
                this,
                &AppSettingsWindow::apply_filter);
  this->connect(this->search,
                &QLineEdit::returnPressed,
                this,
                &AppSettingsWindow::jump_to_best_match);

  auto *find = new QShortcut(QKeySequence::Find, this);
  this->connect(find,
                &QShortcut::activated,
                this,
                [this]()
                {
                  this->search->setFocus(Qt::ShortcutFocusReason);
                  this->search->selectAll();
                });

  for (const QKeySequence &keys :
       {QKeySequence("Ctrl+Return"), QKeySequence("Ctrl+Enter")})
  {
    auto *apply = new QShortcut(keys, this);
    this->connect(apply, &QShortcut::activated, this, &AppSettingsWindow::apply_changes);
  }

  // start in the search box: the fastest way to any setting
  QTimer::singleShot(0, this, [this]() { this->search->setFocus(); });
}

AppSettingsWindow::~AppSettingsWindow() = default;

// ---------------------------------------------------------------------------
// construction
// ---------------------------------------------------------------------------

QVBoxLayout *AppSettingsWindow::add_page(const QString &title, const QString &subtitle)
{
  const int index = this->pages->count();
  this->page_titles.push_back(title);

  // nav entry, with a match counter shown while searching
  auto *nav = new QPushButton(title);
  nav->setObjectName("navButton");
  nav->setCheckable(true);
  nav->setCursor(Qt::PointingHandCursor);
  {
    auto *layout = new QHBoxLayout(nav);
    layout->setContentsMargins(0, 0, 10, 0);
    layout->addStretch(1);
    auto *count = new QLabel(nav);
    count->setObjectName("navCount");
    count->setAttribute(Qt::WA_TransparentForMouseEvents);
    count->setFixedHeight(18);
    count->setAlignment(Qt::AlignCenter);
    count->hide();
    layout->addWidget(count, 0, Qt::AlignVCenter);
  }
  this->nav_group->addButton(nav, index);
  this->nav_layout->addWidget(nav);

  // scrolling page
  auto *scroll = new QScrollArea();
  scroll->setObjectName("pageScroll");
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  auto *content = new QWidget();
  content->setObjectName("pageContent");
  auto *layout = new QVBoxLayout(content);
  layout->setContentsMargins(28, 0, 28, 20);
  layout->setSpacing(6);

  auto *heading = new QLabel(title, content);
  heading->setObjectName("pageTitle");
  layout->addWidget(heading);

  if (!subtitle.isEmpty())
  {
    auto *sub = new QLabel(subtitle, content);
    sub->setObjectName("pageSubtitle");
    sub->setWordWrap(true);
    layout->addWidget(sub);
  }
  layout->addSpacing(10);

  scroll->setWidget(content);
  this->pages->addWidget(scroll);

  if (index == 0)
    nav->setChecked(true);

  return layout;
}

QVBoxLayout *AppSettingsWindow::add_card(QVBoxLayout *page, const QString &heading)
{
  auto card = std::make_unique<Card>();

  if (!heading.isEmpty())
  {
    card->heading = new QLabel(heading.toUpper());
    card->heading->setObjectName("cardHeading");
    card->heading->setProperty("search_text", heading.toLower());
    page->addWidget(card->heading);
  }

  card->frame = new QFrame();
  card->frame->setObjectName("card");
  auto *layout = new QVBoxLayout(card->frame);
  layout->setContentsMargins(6, 4, 6, 4);
  layout->setSpacing(0);
  page->addWidget(card->frame);
  page->addSpacing(8);

  this->cards.push_back(std::move(card));
  return layout;
}

void AppSettingsWindow::add_row(QVBoxLayout   *card_layout,
                                const QString &label,
                                const QString &description,
                                const Control &control,
                                bool           needs_restart,
                                const QString &keywords)
{
  auto  row = std::make_unique<Row>();
  Card *card = this->cards.back().get();

  row->card = card;
  row->page = this->pages->count() - 1;
  row->binding = control.binding;
  row->focus = control.focus ? control.focus : control.widget;
  row->label = label.toLower();
  row->keywords = keywords.toLower();
  row->description = description.toLower();
  row->context = (this->page_titles.back() + " " +
                  (card->heading ? card->heading->property("search_text").toString()
                                 : QString()))
                     .toLower();

  if (row->binding)
  {
    row->binding->restart = needs_restart;
    row->binding->label = label;
  }

  // box = hairline + row, so filtering can drop the hairline of the first
  // visible row in a card
  row->box = new QWidget();
  auto *box_layout = new QVBoxLayout(row->box);
  box_layout->setContentsMargins(0, 0, 0, 0);
  box_layout->setSpacing(0);

  row->divider = new QFrame(row->box);
  row->divider->setObjectName("rowDivider");
  row->divider->setFixedHeight(1);
  row->divider->setVisible(!card->rows.empty());
  box_layout->addWidget(row->divider);

  row->row = new QWidget(row->box);
  row->row->setObjectName("row");
  row->row->setAttribute(Qt::WA_StyledBackground);
  auto *layout = new QHBoxLayout(row->row);
  layout->setContentsMargins(14, 12, 14, 12);
  layout->setSpacing(16);

  auto *text = new QVBoxLayout();
  text->setSpacing(3);

  auto *title_line = new QHBoxLayout();
  title_line->setSpacing(8);

  auto *title = new QLabel(label, row->row);
  title->setObjectName("rowLabel");
  title_line->addWidget(title);

  if (needs_restart)
  {
    auto *tag = new QLabel("Restart", row->row);
    tag->setObjectName("restartTag");
    tag->setToolTip("Takes effect the next time Hesiod starts");
    title_line->addWidget(tag);
  }

  row->modified = new QLabel("Modified", row->row);
  row->modified->setObjectName("modifiedTag");
  row->modified->setToolTip("Changed, not applied yet");
  row->modified->hide();
  title_line->addWidget(row->modified);
  title_line->addStretch(1);
  text->addLayout(title_line);

  if (!description.isEmpty())
  {
    auto *desc = new QLabel(description, row->row);
    desc->setObjectName("rowDescription");
    desc->setWordWrap(true);
    text->addWidget(desc);
  }

  layout->addLayout(text, 1);
  if (control.widget)
    layout->addWidget(control.widget, 0, Qt::AlignVCenter | Qt::AlignRight);

  box_layout->addWidget(row->row);
  card_layout->addWidget(row->box);

  card->rows.push_back(row.get());
  this->rows.push_back(std::move(row));
}

AppSettingsWindow::Control AppSettingsWindow::make_toggle(
    std::function<bool &(AppSettings &)> access,
    unsigned                             effects)
{
  auto *toggle = new ToggleSwitch();
  auto  draft = std::make_shared<bool>(access(HSD_CTX.app_settings));
  toggle->setChecked(*draft);
  toggle->sync();

  Control control;
  control.widget = toggle;
  control.binding = make_binding<bool>(access,
                                       draft,
                                       [toggle](const bool &value)
                                       {
                                         const QSignalBlocker block(toggle);
                                         toggle->setChecked(value);
                                         toggle->sync();
                                       });
  control.binding->effects = effects;

  this->connect(toggle,
                &QAbstractButton::toggled,
                this,
                [this, draft](bool value)
                {
                  *draft = value;
                  this->update_state();
                });
  return control;
}

AppSettingsWindow::Control AppSettingsWindow::make_int(
    std::function<int &(AppSettings &)> access,
    int                                 min,
    int                                 max,
    const QString                      &suffix,
    unsigned                            effects)
{
  auto *spin = new QSpinBox();
  spin->setRange(min, max);
  spin->setSuffix(suffix);
  auto draft = std::make_shared<int>(std::clamp(access(HSD_CTX.app_settings), min, max));
  spin->setValue(*draft);

  Control control;
  control.widget = make_stepper(spin);
  control.focus = spin;
  control.binding = make_binding<int>(access,
                                      draft,
                                      [spin](const int &value)
                                      {
                                        const QSignalBlocker block(spin);
                                        spin->setValue(value);
                                      });
  control.binding->effects = effects;

  this->connect(spin,
                QOverload<int>::of(&QSpinBox::valueChanged),
                this,
                [this, draft](int value)
                {
                  *draft = value;
                  this->update_state();
                });
  return control;
}

AppSettingsWindow::Control AppSettingsWindow::make_choice(
    std::function<int &(AppSettings &)>         access,
    const std::vector<std::pair<int, QString>> &items,
    unsigned                                    effects)
{
  auto *combo = new QComboBox();
  combo->setObjectName("settingsChoice");
  for (const auto &[value, label] : items)
    combo->addItem(label, value);

  auto draft = std::make_shared<int>(access(HSD_CTX.app_settings));
  // a stored value that is not one of the choices still shows as itself
  if (combo->findData(*draft) < 0)
    combo->addItem(QString::number(*draft), *draft);
  combo->setCurrentIndex(combo->findData(*draft));

  Control control;
  control.widget = combo;
  control.focus = combo;
  control.binding = make_binding<int>(access,
                                      draft,
                                      [combo](const int &value)
                                      {
                                        const QSignalBlocker block(combo);
                                        combo->setCurrentIndex(combo->findData(value));
                                      });
  control.binding->effects = effects;

  this->connect(combo,
                QOverload<int>::of(&QComboBox::currentIndexChanged),
                this,
                [this, draft, combo](int index)
                {
                  *draft = combo->itemData(index).toInt();
                  this->update_state();
                });
  return control;
}

AppSettingsWindow::Control AppSettingsWindow::make_color(
    std::function<QColor &(AppSettings &)> access,
    unsigned                               effects)
{
  auto *swatch = new ColorSwatch();
  auto  draft = std::make_shared<QColor>(access(HSD_CTX.app_settings));
  swatch->set_color(*draft);

  Control control;
  control.widget = swatch;
  control.binding = make_binding<QColor>(access,
                                         draft,
                                         [swatch](const QColor &value)
                                         { swatch->set_color(value); });
  control.binding->effects = effects;

  this->connect(swatch,
                &QAbstractButton::clicked,
                this,
                [this, swatch, draft]()
                {
                  const QColor picked = ColorPickerDialog::get_color(*draft,
                                                                     this,
                                                                     "Select color",
                                                                     true);

                  if (!picked.isValid())
                    return;

                  *draft = picked;
                  swatch->set_color(picked);
                  this->update_state();
                });
  return control;
}

AppSettingsWindow::Control AppSettingsWindow::make_scale()
{
  auto *box = new QWidget();
  box->setObjectName("scaleControl");
  auto *layout = new QVBoxLayout(box);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto *spin = new QDoubleSpinBox();
  spin->setRange(ui_scale::kMin, ui_scale::kMax);
  spin->setSingleStep(ui_scale::kStep);
  spin->setDecimals(2);
  spin->setSuffix(" x");

  auto draft = std::make_shared<double>(
      ui_scale::sanitize(HSD_CTX.app_settings.interface.ui_scale));
  spin->setValue(*draft);
  layout->addWidget(make_stepper(spin), 0, Qt::AlignRight);

  // presets under the stepper
  auto *presets = new QHBoxLayout();
  presets->setSpacing(4);
  auto *preset_group = new QButtonGroup(box);
  preset_group->setExclusive(false);

  for (const double v : {0.9, 1.0, 1.1, 1.25, 1.5})
  {
    auto *chip = new QPushButton(QString("%1%").arg(qRound(v * 100)), box);
    chip->setObjectName("presetChip");
    chip->setCheckable(true);
    chip->setCursor(Qt::PointingHandCursor);
    chip->setProperty("scale_value", v);
    preset_group->addButton(chip);
    presets->addWidget(chip);
  }
  layout->addLayout(presets);

  auto sync_chips = [preset_group](double value)
  {
    for (QAbstractButton *chip : preset_group->buttons())
      chip->setChecked(std::abs(chip->property("scale_value").toDouble() - value) < 1e-3);
  };
  sync_chips(*draft);

  Control control;
  control.widget = box;
  control.focus = spin;
  control.binding = make_binding<double>(
      HSD_SETTING(double, interface.ui_scale),
      draft,
      [spin, sync_chips](const double &value)
      {
        const QSignalBlocker block(spin);
        spin->setValue(value);
        sync_chips(value);
      },
      [](const double &a, const double &b)
      { return std::abs(ui_scale::sanitize(a) - ui_scale::sanitize(b)) < 1e-6; });
  control.binding->effects = Scale;
  control.binding->restart = !ui_scale::live_apply_supported();

  this->connect(spin,
                QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this,
                [this, draft, sync_chips](double value)
                {
                  *draft = ui_scale::sanitize(value);
                  sync_chips(value);
                  this->update_state();
                });

  this->connect(preset_group,
                &QButtonGroup::buttonClicked,
                this,
                [spin, sync_chips](QAbstractButton *chip)
                {
                  const double value = chip->property("scale_value").toDouble();
                  spin->setValue(value);
                  sync_chips(spin->value()); // an unchecking click re-checks
                });
  return control;
}

void AppSettingsWindow::setup_pages()
{
  // --- General

  {
    QVBoxLayout *page = this->add_page("General", "Saving, recovery and performance.");

    QVBoxLayout *files = this->add_card(page, "Files");
    this->add_row(files,
                  "Back up on save",
                  "Keep a backup copy of the project file every time it is saved.",
                  this->make_toggle(HSD_SETTING(bool, global.save_backup_file)),
                  false,
                  "bak copy previous version");

    QVBoxLayout *recovery = this->add_card(page, "Crash recovery");
    this->add_row(recovery,
                  "Autosave",
                  "Snapshot unsaved work in the background so it can be restored "
                  "after a crash.",
                  this->make_toggle(HSD_SETTING(bool, global.enable_autosave), Autosave),
                  false,
                  "recovery crash snapshot restore");
    this->add_row(recovery,
                  "Autosave interval",
                  "Time between two snapshots.",
                  this->make_int(HSD_SETTING(int, global.autosave_interval_s),
                                 10,
                                 3600,
                                 " s",
                                 Autosave),
                  false,
                  "seconds frequency period timer");

    QVBoxLayout *perf = this->add_card(page, "Performance");
    this->add_row(perf,
                  "CPU threads",
                  "Threads used for OpenMP computations. -1 lets CLWrapper decide.",
                  this->make_int(HSD_SETTING(int, global.omp_num_threads), -1, 64),
                  true,
                  "openmp cores parallel speed");
  }

  // --- Interface

  {
    QVBoxLayout *page = this->add_page("Interface", "Scale, behavior and extra tools.");

    QVBoxLayout *scale = this->add_card(page, "Scale");
    this->add_row(scale,
                  "Interface scale",
                  "Text, icons, spacing and controls. Multiplied with the monitor's "
                  "own scaling.",
                  this->make_scale(),
                  !ui_scale::live_apply_supported(),
                  "zoom size dpi bigger smaller ui font hidpi");

    // environment override: say so under the scale row
    {
      const auto session = ui_scale::session_resolution();
      if (session.environment_override)
      {
        this->scale_note = new QLabel(
            QString("QT_SCALE_FACTOR is set in the environment, so Hesiod runs at "
                    "%1x and ignores this setting.")
                .arg(session.effective, 0, 'f', 2));
        this->scale_note->setObjectName("rowDescription");
        this->scale_note->setWordWrap(true);
        this->scale_note->setContentsMargins(14, 0, 14, 12);
        scale->addWidget(this->scale_note);
      }
    }

    QVBoxLayout *behavior = this->add_card(page, "Behavior");
    this->add_row(
        behavior,
        "Interface animations",
        "Menus, switches and panels animate when they change.",
        this->make_toggle(HSD_SETTING(bool, interface.enable_ui_animations), Animations),
        false,
        "motion transitions effects fade");
    this->add_row(behavior,
                  "Tooltips",
                  "Show hints when hovering controls.",
                  this->make_toggle(HSD_SETTING(bool, interface.enable_tool_tips)),
                  false,
                  "hints help hover popup");
    this->add_row(behavior,
                  "Project browser at startup",
                  "Open the welcome window with recent projects and examples.",
                  this->make_toggle(
                      HSD_SETTING(bool, interface.enable_example_selector_at_startup)),
                  false,
                  "welcome launch examples recent start");

    QVBoxLayout *nodes = this->add_card(page, "Node bodies");
    this->add_row(
        nodes,
        "Data preview",
        "Draw a thumbnail of each node's output inside the node.",
        this->make_toggle(HSD_SETTING(bool, interface.enable_data_preview_in_node_body)),
        false,
        "thumbnail image picture");
    this->add_row(
        nodes,
        "Inline settings",
        "Show node parameters inside the node body.",
        this->make_toggle(HSD_SETTING(bool, interface.enable_node_settings_in_node_body)),
        false,
        "parameters attributes controls");

    QVBoxLayout *tools = this->add_card(page, "Tools");
    this->add_row(
        tools,
        "Texture downloader",
        "Adds the texture downloader to the View menu.",
        this->make_toggle(HSD_SETTING(bool, interface.enable_texture_downloader)),
        true,
        "textures download materials");
  }

  // --- Node editor

  {
    QVBoxLayout *page = this->add_page("Node Editor",
                                       "Graph editing and the 3D viewport.");

    QVBoxLayout *editing = this->add_card(page, "Editing");
    this->add_row(editing,
                  "Live update",
                  "Recompute the graph while a value is being edited.",
                  this->make_toggle(HSD_SETTING(bool, node_editor.live_update)),
                  false,
                  "realtime compute refresh");
    this->add_row(editing,
                  "Starting resolution",
                  "Heightmap resolution of new projects and new graphs. Change a "
                  "graph's own resolution from the viewport toolbar.",
                  this->make_choice(HSD_SETTING(int, node_editor.default_resolution),
                                    {{256, "256 × 256"},
                                     {512, "512 × 512"},
                                     {1024, "1024 × 1024"},
                                     {2048, "2048 × 2048"},
                                     {4096, "4096 × 4096"}}),
                  false,
                  "default heightmap size shape new graph project resolution");

    QVBoxLayout *panels = this->add_card(page, "Panels");
    this->add_row(panels,
                  "Node toolbar in settings panel",
                  "Show the node actions at the top of the settings panel.",
                  this->make_toggle(
                      HSD_SETTING(bool, node_editor.show_node_toolbar_in_settings_pan)),
                  false,
                  "actions buttons");
    this->add_row(panels,
                  "Node library panel",
                  "Show the library next to the graph.",
                  this->make_toggle(HSD_SETTING(bool, node_editor.show_node_library_pan)),
                  false,
                  "nodes list tree sidebar");

    QVBoxLayout *viewport = this->add_card(page, "Viewport");
    this->add_row(viewport,
                  "Heightmap skirt",
                  "Close the sides of the terrain mesh down to its base.",
                  this->make_toggle(HSD_SETTING(bool, viewer.add_heighmap_skirt)),
                  false,
                  "3d mesh border sides base viewer");
    this->add_row(viewport,
                  "Toolbar size",
                  "Size of the viewport's floating toolbar, as a percentage.",
                  this->make_int(HSD_SETTING(int, viewer.toolbar_scale),
                                 50,
                                 200,
                                 " %",
                                 ViewportToolbar),
                  false,
                  "viewport toolbar rail zoom scale bigger smaller icons");
  }

  // --- Node palette

  {
    QVBoxLayout *page = this->add_page(
        "Node Palette",
        "The category rail: an alternative to the node library tree, with "
        "hierarchical flyout menus.");

    QVBoxLayout *mode = this->add_card(page, "Mode");
    this->add_row(
        mode,
        "Use the category rail",
        "Replace the library tree with the rail.",
        this->make_toggle(HSD_SETTING(bool, interface.enable_node_palette_sidebar)),
        true,
        "sidebar categories flyout");

    QVBoxLayout *colors = this->add_card(page, "Colors");
    const auto   color_row =
        [&](const QString &label, std::function<QColor &(AppSettings &)> access)
    {
      this->add_row(colors,
                    label,
                    QString(),
                    this->make_color(access, Theme | Palette),
                    false,
                    "colour theme");
    };
    color_row("Category button", HSD_SETTING(QColor, node_palette.surface));
    color_row("Category button, hovered",
              HSD_SETTING(QColor, node_palette.surface_hover));
    color_row("Category button, open",
              HSD_SETTING(QColor, node_palette.surface_selected));
    color_row("Flyout background", HSD_SETTING(QColor, node_palette.flyout_bg));
    color_row("Flyout border", HSD_SETTING(QColor, node_palette.flyout_border));
    color_row("Label", HSD_SETTING(QColor, node_palette.text));
    color_row("Label, active", HSD_SETTING(QColor, node_palette.text_active));

    QVBoxLayout *metrics = this->add_card(page, "Layout");
    const auto   metric_row = [&](const QString                      &label,
                                std::function<int &(AppSettings &)> access,
                                int                                 min,
                                int                                 max,
                                const QString                      &suffix)
    {
      this->add_row(metrics,
                    label,
                    QString(),
                    this->make_int(access, min, max, suffix, Palette),
                    false,
                    "size metrics");
    };
    metric_row("Button height",
               HSD_SETTING(int, node_palette.button_height),
               20,
               120,
               " px");
    metric_row("Button spacing",
               HSD_SETTING(int, node_palette.button_spacing),
               0,
               40,
               " px");
    metric_row("Rail padding", HSD_SETTING(int, node_palette.rail_padding), 0, 40, " px");
    metric_row("Icon size", HSD_SETTING(int, node_palette.icon_size), 8, 64, " px");
    metric_row("Corner radius",
               HSD_SETTING(int, node_palette.corner_radius),
               0,
               24,
               " px");
    metric_row("Flyout row height",
               HSD_SETTING(int, node_palette.flyout_row_height),
               14,
               80,
               " px");
    metric_row("Flyout padding",
               HSD_SETTING(int, node_palette.flyout_padding),
               0,
               24,
               " px");
    metric_row("Animation duration",
               HSD_SETTING(int, node_palette.animation_ms),
               0,
               1000,
               " ms");
  }

  // pages end with a stretch so short pages stay top-aligned
  for (int i = 0; i < this->pages->count(); ++i)
    if (auto *scroll = qobject_cast<QScrollArea *>(this->pages->widget(i)))
      if (auto *layout = qobject_cast<QVBoxLayout *>(scroll->widget()->layout()))
        layout->addStretch(1);
}

// ---------------------------------------------------------------------------
// draft handling
// ---------------------------------------------------------------------------

int AppSettingsWindow::dirty_count() const
{
  int count = 0;
  for (const auto &row : this->rows)
    if (row->binding && row->binding->dirty())
      ++count;
  return count;
}

void AppSettingsWindow::update_state()
{
  // per-row marker, per-section marker, footer
  std::vector<bool> page_dirty(this->pages->count(), false);
  int               count = 0;

  for (const auto &row : this->rows)
  {
    const bool dirty = row->binding && row->binding->dirty();
    row->modified->setVisible(dirty);
    if (row->row->property("modified").toBool() != dirty)
    {
      row->row->setProperty("modified", dirty);
      repolish(row->row);
    }
    if (dirty)
    {
      page_dirty[row->page] = true;
      ++count;
    }
  }

  for (int i = 0; i < int(page_dirty.size()); ++i)
    if (QAbstractButton *nav = this->nav_group->button(i))
      if (nav->property("modified").toBool() != page_dirty[i])
      {
        nav->setProperty("modified", bool(page_dirty[i]));
        repolish(nav);
      }

  this->apply_button->setEnabled(count > 0);
  this->discard_button->setEnabled(count > 0);
  this->footer_status->setText(count == 0   ? QString()
                               : count == 1 ? QString("1 unapplied change")
                                            : QString("%1 unapplied changes").arg(count));

  this->restart_banner->setVisible(!g_restart_pending.isEmpty());
  if (!g_restart_pending.isEmpty())
    this->restart_label->setText(QString("Restart Hesiod to finish applying: %1.")
                                     .arg(g_restart_pending.join(", ")));
}

void AppSettingsWindow::apply_changes()
{
  unsigned effects = NoEffect;
  int      applied = 0;

  for (const auto &row : this->rows)
  {
    if (!row->binding || !row->binding->dirty())
      continue;

    row->binding->commit();
    effects |= row->binding->effects;
    ++applied;

    if (row->binding->restart && !g_restart_pending.contains(row->binding->label))
      g_restart_pending.push_back(row->binding->label);
  }

  if (applied == 0)
    return;

  AppContext &ctx = HSD_CTX;

  if (effects & Autosave)
    if (AutosaveManager *autosave = HSD_APP->get_autosave_manager_ref())
    {
      autosave->set_enabled(ctx.app_settings.global.enable_autosave);
      autosave->set_interval(
          std::chrono::seconds(ctx.app_settings.global.autosave_interval_s));
    }

  if (effects & Animations)
    apply_animation_settings(ctx.app_settings.interface.enable_ui_animations);

  if (effects & Theme)
    if (auto *app = qobject_cast<QApplication *>(QCoreApplication::instance()))
      apply_global_style(*app);

  if (effects & (Theme | Palette))
    NodePaletteSidebar::restyle_all(current_node_palette_style());

  if (effects & ViewportToolbar)
    ViewportControls::relayout_all();

  // persist now rather than on quit: a crash after applying should not lose it
  ctx.save_settings();

  if (effects & Scale)
  {
    ui_scale::apply_live(ctx.app_settings.interface.ui_scale);
    // the dialog keeps its logical size, so a larger scale makes it physically
    // larger: keep it on screen
    QTimer::singleShot(0, this, [this]() { this->fit_to_screen(); });
  }

  this->update_state();
  this->footer_status->setText(applied == 1 ? QString("Applied 1 change")
                                            : QString("Applied %1 changes").arg(applied));
}

void AppSettingsWindow::discard_changes()
{
  for (const auto &row : this->rows)
    if (row->binding && row->binding->dirty())
      row->binding->revert();

  this->update_state();
}

void AppSettingsWindow::load_defaults()
{
  for (const auto &row : this->rows)
    if (row->binding)
      row->binding->load_default();

  this->update_state();

  const int count = this->dirty_count();
  this->footer_status->setText(count == 0 ? QString("Already at defaults")
                                          : QString("Defaults filled in: review, then "
                                                    "Apply (%1)")
                                                .arg(count));
}

bool AppSettingsWindow::confirm_close()
{
  const int count = this->dirty_count();
  if (count == 0)
    return true;

  MessageDialog box(this,
                    MessageDialog::Kind::Question,
                    count == 1 ? QString("Apply 1 change?")
                               : QString("Apply %1 changes?").arg(count),
                    "Some settings were changed but not applied.");
  QPushButton  *discard = box.add_button("Discard changes", MessageDialog::Role::Danger);
  QPushButton  *keep = box.add_button("Keep editing",
                                     MessageDialog::Role::Secondary,
                                     false,
                                     true);
  QPushButton  *apply = box.add_button("Apply", MessageDialog::Role::Primary, true);
  box.exec();

  if (box.clicked_button() == apply)
  {
    this->apply_changes();
    return true;
  }
  if (box.clicked_button() == discard)
  {
    this->discard_changes();
    return true;
  }

  Q_UNUSED(keep);
  return false; // keep editing (or the prompt was dismissed)
}

void AppSettingsWindow::done(int result)
{
  if (!this->confirm_close())
    return;

  QDialog::done(result);
}

// ---------------------------------------------------------------------------
// search
// ---------------------------------------------------------------------------

void AppSettingsWindow::apply_filter(const QString &query)
{
  static const QRegularExpression spaces("\\s+");
  const QStringList tokens = query.trimmed().toLower().split(spaces, Qt::SkipEmptyParts);

  this->best_match = nullptr;
  int best_score = 0;

  std::vector<int> page_hits(this->pages->count(), 0);
  std::vector<int> page_best(this->pages->count(), 0);

  for (const auto &row : this->rows)
  {
    int score = 0;

    if (!tokens.isEmpty())
    {
      for (const QString &token : tokens)
      {
        // label counts most; description only on real words, not scattered letters
        const int s = std::max({token_score(token, row->label, true),
                                token_score(token, row->keywords, true) * 9 / 10,
                                token_score(token, row->context, false) * 6 / 10,
                                token_score(token, row->description, false) / 2});
        if (s == 0)
        {
          score = 0;
          break;
        }
        score += s;
      }
    }

    const bool visible = tokens.isEmpty() || score > 0;
    row->box->setVisible(visible);

    if (visible && !tokens.isEmpty())
    {
      ++page_hits[row->page];
      page_best[row->page] = std::max(page_best[row->page], score);
      if (score > best_score)
      {
        best_score = score;
        this->best_match = row.get();
      }
    }
  }

  // cards: hide empty ones and the hairline above their first visible row
  for (const auto &card : this->cards)
  {
    bool first = true;
    for (Row *row : card->rows)
      if (row->box->isVisibleTo(card->frame))
      {
        row->divider->setVisible(!first);
        first = false;
      }

    card->frame->setVisible(!first);
    if (card->heading)
      card->heading->setVisible(!first);
  }

  // nav: match counts, sections without matches dimmed
  int total = 0;
  for (int i = 0; i < this->pages->count(); ++i)
  {
    QAbstractButton *nav = this->nav_group->button(i);
    auto            *count = nav->findChild<QLabel *>("navCount");
    total += page_hits[i];

    if (tokens.isEmpty())
    {
      nav->setEnabled(true);
      count->hide();
    }
    else
    {
      nav->setEnabled(page_hits[i] > 0);
      count->setText(QString::number(page_hits[i]));
      count->setVisible(page_hits[i] > 0);
    }
  }

  if (tokens.isEmpty())
  {
    this->search_status->hide();
    return;
  }

  // stay on the current section if it has results, else go to the best one
  if (page_hits[this->pages->currentIndex()] == 0 && this->best_match)
    if (QAbstractButton *nav = this->nav_group->button(this->best_match->page))
      nav->click();

  this->search_status->setText(
      total == 0   ? QString("No settings match \"%1\".").arg(query.trimmed())
      : total == 1 ? QString("1 match. Enter to jump to it.")
                   : QString("%1 matches. Enter jumps to the best one.").arg(total));
  this->search_status->show();
}

void AppSettingsWindow::jump_to_best_match()
{
  Row *row = this->best_match;
  if (!row)
    return;

  if (QAbstractButton *nav = this->nav_group->button(row->page))
    nav->click();

  if (auto *scroll = qobject_cast<QScrollArea *>(this->pages->widget(row->page)))
    scroll->ensureWidgetVisible(row->row, 0, 60);

  // brief highlight so the eye finds the row
  row->row->setProperty("hit", true);
  repolish(row->row);
  QPointer<QWidget> target = row->row;
  QTimer::singleShot(1200,
                     this,
                     [target]()
                     {
                       if (!target)
                         return;
                       target->setProperty("hit", false);
                       repolish(target);
                     });

  if (row->focus)
    row->focus->setFocus(Qt::ShortcutFocusReason);
}

// ---------------------------------------------------------------------------
// chrome
// ---------------------------------------------------------------------------

void AppSettingsWindow::apply_stylesheet()
{
  const Tones t = tones();

  QString css = R"(
    QDialog#hsdSettings QWidget { background: transparent; color: INK; }
    QDialog#hsdSettings QLabel#sidebarTitle { font-size: 15px; font-weight: 600; }
    QDialog#hsdSettings QLabel#sidebarVersion { color: FAINT; font-size: 11px; }
    QDialog#hsdSettings QLabel#searchStatus { color: FAINT; font-size: 11px; padding: 0px 8px; }

    QDialog#hsdSettings QLineEdit#settingsSearch {
      background: FIELD; border: 1px solid BORDER; border-radius: 7px;
      padding: 6px 6px; font-size: 13px; color: INK;
      selection-background-color: ACCENT; }
    QDialog#hsdSettings QLineEdit#settingsSearch:focus { border-color: ACCENT; }

    QDialog#hsdSettings QPushButton#navButton {
      text-align: left; padding: 8px 12px; border: none; border-radius: 7px;
      color: DIM; font-size: 13px; background: transparent; }
    QDialog#hsdSettings QPushButton#navButton:hover { background: NAV_HOVER; color: INK; }
    QDialog#hsdSettings QPushButton#navButton:checked {
      background: NAV_ACTIVE; color: INK; font-weight: 600; }
    QDialog#hsdSettings QPushButton#navButton[modified="true"] { color: ACCENT_TEXT; }
    QDialog#hsdSettings QPushButton#navButton:disabled { color: GHOST; }
    QDialog#hsdSettings QLabel#navCount {
      background: ACCENT_SOFT; color: INK; border-radius: 8px;
      padding: 0px 7px; font-size: 11px; font-weight: 600; }

    QDialog#hsdSettings QLabel#pageTitle { font-size: 22px; font-weight: 600; }
    QDialog#hsdSettings QLabel#pageSubtitle { color: DIM; font-size: 13px; }
    QDialog#hsdSettings QLabel#cardHeading {
      color: FAINT; font-size: 11px; font-weight: 600; letter-spacing: 1px;
      padding: 8px 0px 4px 4px; }

    QDialog#hsdSettings QFrame#card {
      background: CARD; border: 1px solid BORDER; border-radius: 10px; }
    QDialog#hsdSettings QFrame#rowDivider { background: BORDER; border: none; }
    QDialog#hsdSettings QWidget#row { border-radius: 8px; }
    QDialog#hsdSettings QWidget#row[modified="true"] { background: MODIFIED_BG; }
    QDialog#hsdSettings QWidget#row[hit="true"] { background: ACCENT_SOFT; }
    QDialog#hsdSettings QLabel#rowLabel { font-size: 13px; }
    QDialog#hsdSettings QLabel#rowDescription { color: DIM; font-size: 12px; }
    QDialog#hsdSettings QLabel#restartTag {
      color: WARN; background: WARN_SOFT; border-radius: 4px;
      padding: 1px 6px; font-size: 10px; font-weight: 600; }
    QDialog#hsdSettings QLabel#modifiedTag {
      color: ACCENT_TEXT; background: ACCENT_SOFT; border-radius: 4px;
      padding: 1px 6px; font-size: 10px; font-weight: 600; }

    QDialog#hsdSettings QFrame#restartBanner {
      background: WARN_SOFT; border: 1px solid WARN_BORDER; border-radius: 8px; }
    QDialog#hsdSettings QLabel#restartLabel { color: INK; font-size: 12px; }

    QDialog#hsdSettings QFrame#stepper {
      background: FIELD; border: 1px solid BORDER; border-radius: 7px; }
    QDialog#hsdSettings QFrame#stepper:hover { border-color: FAINT; }
    QDialog#hsdSettings QAbstractSpinBox {
      background: transparent; border: none; color: INK; font-size: 13px;
      selection-background-color: ACCENT; padding: 0px; min-height: 28px; }

    QDialog#hsdSettings QComboBox#settingsChoice {
      background: FIELD; border: 1px solid BORDER; border-radius: 7px; color: INK;
      font-size: 13px; padding: 4px 10px; min-height: 22px; min-width: 110px; }
    QDialog#hsdSettings QComboBox#settingsChoice:hover { border-color: FAINT; }
    QDialog#hsdSettings QComboBox#settingsChoice::drop-down { border: none; width: 20px; }

    QDialog#hsdSettings QPushButton#presetChip {
      background: FIELD; color: DIM; border: 1px solid BORDER; border-radius: 12px;
      padding: 3px 9px; font-size: 11px; }
    QDialog#hsdSettings QPushButton#presetChip:hover { color: INK; border-color: FAINT; }
    QDialog#hsdSettings QPushButton#presetChip:checked {
      background: ACCENT_SOFT; color: INK; border-color: ACCENT; }

    QDialog#hsdSettings QPushButton#primaryButton {
      background: ACCENT; color: #ffffff; border: 1px solid ACCENT; border-radius: 7px;
      padding: 7px 22px; font-size: 13px; font-weight: 600; }
    QDialog#hsdSettings QPushButton#primaryButton:hover { background: ACCENT_HOVER; }
    QDialog#hsdSettings QPushButton#primaryButton:disabled {
      background: FIELD; border-color: BORDER; color: GHOST; }
    QDialog#hsdSettings QPushButton#secondaryButton {
      background: FIELD; color: INK; border: 1px solid BORDER; border-radius: 7px;
      padding: 7px 16px; font-size: 13px; }
    QDialog#hsdSettings QPushButton#secondaryButton:hover { border-color: FAINT; }
    QDialog#hsdSettings QPushButton#secondaryButton:disabled { color: GHOST; }
    QDialog#hsdSettings QPushButton#ghostButton {
      background: transparent; color: DIM; border: 1px solid transparent;
      border-radius: 7px; padding: 7px 12px; font-size: 13px; }
    QDialog#hsdSettings QPushButton#ghostButton:hover { background: FIELD; color: INK; }
    QDialog#hsdSettings QLabel#footerStatus { color: DIM; font-size: 12px; }

    QDialog#hsdSettings QScrollBar:vertical {
      width: 10px; background: transparent; margin: 4px 2px 4px 0px; border: none; }
    QDialog#hsdSettings QScrollBar::handle:vertical {
      background: BORDER; border-radius: 4px; min-height: 36px; }
    QDialog#hsdSettings QScrollBar::handle:vertical:hover { background: FAINT; }
    QDialog#hsdSettings QScrollBar::add-line:vertical,
    QDialog#hsdSettings QScrollBar::sub-line:vertical { height: 0px; border: none; }
    QDialog#hsdSettings QScrollBar::add-page:vertical,
    QDialog#hsdSettings QScrollBar::sub-page:vertical { background: transparent; }
  )";

  const QColor                          warn("#d9a441");
  const std::pair<const char *, QColor> tokens[] = {
      {"ACCENT_HOVER", t.accent.lighter(112)},
      {"ACCENT_SOFT", t.accent_soft},
      {"ACCENT_TEXT", t.accent.lighter(135)},
      {"ACCENT", t.accent},
      {"MODIFIED_BG", mix(t.card, t.accent, 0.08)},
      {"NAV_ACTIVE", mix(t.sidebar, t.card, 0.85)},
      {"NAV_HOVER", mix(t.sidebar, t.card, 0.45)},
      {"WARN_SOFT", mix(t.content, warn, 0.14)},
      {"WARN_BORDER", mix(t.content, warn, 0.35)},
      {"WARN", warn},
      {"CARD", t.card},
      {"BORDER", t.border},
      {"FIELD", t.field},
      {"GHOST", mix(t.sidebar, t.ink, 0.28)},
      {"FAINT", t.ink_faint},
      {"DIM", t.ink_dim},
      {"INK", t.ink}};
  for (const auto &[key, value] : tokens)
    css.replace(key, value.name());

  const QString family = meta::qt::ui_font(13).family();
  if (!family.isEmpty())
    css.prepend(
        QString("QDialog#hsdSettings QWidget { font-family: \"%1\"; }\n").arg(family));

  this->setStyleSheet(css);
}

void AppSettingsWindow::fit_to_screen()
{
  QScreen *screen = this->screen() ? this->screen() : QGuiApplication::primaryScreen();
  if (!screen)
    return;

  const QRect avail = screen->availableGeometry();
  const QSize target = QSize(940, 680).boundedTo(avail.size() * 0.92);
  this->resize(target);

  // recentre over the parent (or the screen) after a resize
  const QRect anchor = this->parentWidget() && this->parentWidget()->window()->isVisible()
                           ? this->parentWidget()->window()->geometry()
                           : avail;
  QRect       geom(QPoint(0, 0), target);
  geom.moveCenter(anchor.center());
  geom.moveLeft(std::clamp(geom.left(), avail.left(), avail.right() - geom.width()));
  geom.moveTop(std::clamp(geom.top(), avail.top(), avail.bottom() - geom.height()));
  this->move(geom.topLeft());
}

void AppSettingsWindow::keyPressEvent(QKeyEvent *event)
{
  if (event->key() == Qt::Key_Escape)
  {
    // Escape peels one layer at a time: search text first, then the dialog
    if (!this->search->text().isEmpty())
    {
      this->search->clear();
      this->search->setFocus();
      return;
    }
    this->reject(); // done() asks about unapplied changes
    return;
  }

  // Enter must not press a default button: in a settings form it would apply
  // by accident while the user is typing a number
  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
    return;

  QDialog::keyPressEvent(event);
}

void AppSettingsWindow::mousePressEvent(QMouseEvent *event)
{
  if (event->button() == Qt::LeftButton && event->position().y() < kHeaderH)
  {
    this->dragging = true;
    this->drag_offset = event->globalPosition().toPoint() -
                        this->frameGeometry().topLeft();
    event->accept();
    return;
  }
  QDialog::mousePressEvent(event);
}

void AppSettingsWindow::mouseMoveEvent(QMouseEvent *event)
{
  if (this->dragging && (event->buttons() & Qt::LeftButton))
  {
    this->move(event->globalPosition().toPoint() - this->drag_offset);
    event->accept();
    return;
  }
  QDialog::mouseMoveEvent(event);
}

void AppSettingsWindow::mouseReleaseEvent(QMouseEvent *event)
{
  this->dragging = false;
  QDialog::mouseReleaseEvent(event);
}

void AppSettingsWindow::paintEvent(QPaintEvent *)
{
  const Tones t = tones();
  QPainter    p(this);
  p.setRenderHint(QPainter::Antialiasing);

  const QRectF outer = QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5);
  QPainterPath shape;
  shape.addRoundedRect(outer, kRadius, kRadius);

  p.fillPath(shape, t.content);

  // sidebar, clipped to the rounded outline
  p.save();
  p.setClipPath(shape);
  p.fillRect(QRectF(0, 0, kSidebarW + 1, this->height()), t.sidebar);
  p.setPen(QPen(t.border, 1));
  p.drawLine(QPointF(kSidebarW + 1.5, 0), QPointF(kSidebarW + 1.5, this->height()));
  p.restore();

  p.setPen(QPen(t.border, 1));
  p.setBrush(Qt::NoBrush);
  p.drawPath(shape);
}

} // namespace hesiod
