/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <functional>

#include <QCheckBox>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFrame>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariantAnimation>
#include <QWheelEvent>

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/example_selector_dialog.hpp"
#include "hesiod/gui/widgets/properties_panel_design.hpp"
#include "hesiod/logger.hpp"
#include "meta_qt/ui/theme.hpp"

namespace hesiod
{

namespace
{

QColor blend_color(const QColor &from, const QColor &to, qreal amount)
{
  amount = std::clamp(amount, 0.0, 1.0);
  return QColor(qRound(from.red() + (to.red() - from.red()) * amount),
                qRound(from.green() + (to.green() - from.green()) * amount),
                qRound(from.blue() + (to.blue() - from.blue()) * amount),
                qRound(from.alpha() + (to.alpha() - from.alpha()) * amount));
}

meta::qt::Theme selector_theme()
{
  const auto &design = properties_panel_design();
  return design.has_own_chrome ? *design.theme
                               : meta::qt::Theme::from_palette(QApplication::palette());
}

QIcon search_icon()
{
  // Render above display resolution; QIcon chooses the correct device scale.
  QPixmap pixmap(64, 64);
  pixmap.fill(Qt::transparent);
  pixmap.setDevicePixelRatio(4);
  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(QPen(selector_theme().ink_secondary, 1.5));
  painter.drawEllipse(QRectF(2.5, 2.5, 7.5, 7.5));
  painter.drawLine(QPointF(9.2, 9.2), QPointF(13.4, 13.4));
  return QIcon(pixmap);
}

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

QString file_size_label(qint64 bytes)
{
  if (bytes < 1024 * 1024)
    return QString::number(std::max<qint64>(1, bytes / 1024)) + " KB";
  return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + " MB";
}

class DialogTitleBar final : public QWidget
{
public:
  explicit DialogTitleBar(QDialog *dialog) : QWidget(dialog), dialog(dialog)
  {
    this->setObjectName("selectorTitleBar");
    this->setFixedHeight(48);
  }

protected:
  void mousePressEvent(QMouseEvent *event) override
  {
    if (event->button() == Qt::LeftButton)
    {
      this->dragging = true;
      this->drag_offset = event->globalPosition().toPoint() -
                          this->dialog->frameGeometry().topLeft();
      event->accept();
      return;
    }
    QWidget::mousePressEvent(event);
  }

  void mouseMoveEvent(QMouseEvent *event) override
  {
    if (this->dragging && (event->buttons() & Qt::LeftButton))
    {
      this->dialog->move(event->globalPosition().toPoint() - this->drag_offset);
      event->accept();
      return;
    }
    QWidget::mouseMoveEvent(event);
  }

  void mouseReleaseEvent(QMouseEvent *event) override
  {
    this->dragging = false;
    QWidget::mouseReleaseEvent(event);
  }

private:
  QDialog *dialog = nullptr;
  QPoint   drag_offset;
  bool     dragging = false;
};

class SmoothScrollArea final : public QScrollArea
{
public:
  explicit SmoothScrollArea(QWidget *parent = nullptr) : QScrollArea(parent)
  {
    this->scroll_animation = new QVariantAnimation(this);
    QObject::connect(this->verticalScrollBar(),
                     &QScrollBar::sliderPressed,
                     this->scroll_animation,
                     &QVariantAnimation::stop);
    QObject::connect(this->scroll_animation,
                     &QVariantAnimation::valueChanged,
                     this,
                     [this](const QVariant &value)
                     { this->verticalScrollBar()->setValue(value.toInt()); });
  }

protected:
  void wheelEvent(QWheelEvent *event) override
  {
    // Precision touchpads already deliver smooth pixel deltas. Only animate
    // discrete mouse-wheel steps, where the stock table used to jump a row.
    if (!event->pixelDelta().isNull() || event->angleDelta().y() == 0)
    {
      this->scroll_animation->stop();
      QScrollArea::wheelEvent(event);
      return;
    }

    QScrollBar *bar = this->verticalScrollBar();
    const int   base = this->scroll_animation->state() == QAbstractAnimation::Running
                           ? this->scroll_animation->endValue().toInt()
                           : bar->value();
    const qreal steps = event->angleDelta().y() / 120.0;
    const int   target = std::clamp(base - qRound(steps * 150),
                                  bar->minimum(),
                                  bar->maximum());

    this->scroll_animation->stop();
    this->scroll_animation->setStartValue(bar->value());
    this->scroll_animation->setEndValue(target);
    this->scroll_animation->setDuration(170);
    this->scroll_animation->setEasingCurve(QEasingCurve::OutCubic);
    this->scroll_animation->start();
    event->accept();
  }

private:
  QVariantAnimation *scroll_animation = nullptr;
};

// Paint directly from the source image at the destination device scale.
// Pre-scaling to logical pixels loses detail on 150% and 200% displays.
void draw_preview(QPainter &painter, const QPixmap &image, const QRectF &target)
{
  if (image.isNull())
    return;
  const qreal  scale = std::max(target.width() / image.width(),
                               target.height() / image.height());
  const QSizeF source_size(target.width() / scale, target.height() / scale);
  const QRectF source((image.width() - source_size.width()) / 2,
                      (image.height() - source_size.height()) / 2,
                      source_size.width(),
                      source_size.height());
  painter.setRenderHint(QPainter::SmoothPixmapTransform);
  painter.drawPixmap(target, image, source);
}

class WelcomeCanvas final : public QWidget
{
public:
  explicit WelcomeCanvas(QWidget *parent) : QWidget(parent) {}

protected:
  void paintEvent(QPaintEvent *) override
  {
    const auto t = selector_theme();
    QPainter   p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), t.page);
    // The application's own icon as a watermark, rather than a chevron drawn
    // here. Hand drawing it means the welcome screen carries a second, subtly
    // different mark that has to be kept in step with the real one by hand;
    // loading the asset means it simply is the logo.
    //
    // Requested at low opacity rather than by blending a pen colour, because
    // the asset is a full colour image and there is no single stroke colour to
    // tint. Rendered above display resolution so it stays clean when scaled.
    const QString icon_path = QString::fromStdString(
        HSD_CTX.app_settings.global.icon_path);
    const QIcon mark(icon_path);

    if (!mark.isNull())
    {
      const int   side = int(std::min(width() * 0.52, height() * 0.62));
      const QRect target(int(width() * 0.02), int(height() * 0.30), side, side);

      p.setOpacity(0.06);
      p.drawPixmap(target, mark.pixmap(target.size() * 2, this->devicePixelRatioF()));
      p.setOpacity(1.0);
    }
  }
};

class RecentProjectRow final : public QPushButton
{
public:
  RecentProjectRow(const QString        &title,
                   const QString        &detail,
                   std::function<void()> activate,
                   QWidget              *parent)
      : QPushButton(parent), title(title), detail(detail)
  {
    setObjectName("selectorRecentRow");
    setFixedHeight(62);
    setCursor(Qt::PointingHandCursor);
    setAccessibleName(title);
    setAccessibleDescription(detail);
    setAutoDefault(false);
    connect(this, &QPushButton::clicked, this, std::move(activate));
  }

protected:
  void paintEvent(QPaintEvent *) override
  {
    const auto t = selector_theme();
    QPainter   p(this);
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    p.setBrush(underMouse() ? t.field_hover : t.bar);
    p.setPen(QPen(hasFocus() ? t.accent : t.field_border, 1));
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5),
                      t.metrics.section_card_radius,
                      t.metrics.section_card_radius);
    p.setFont(meta::qt::ui_font(14));
    p.setPen(t.ink_primary);
    p.drawText(QRect(14, 8, width() - 42, 23),
               Qt::AlignVCenter,
               p.fontMetrics().elidedText(title, Qt::ElideRight, width() - 42));
    p.setFont(meta::qt::ui_font(12));
    p.setPen(t.ink_secondary);
    p.drawText(QRect(14, 33, width() - 42, 21),
               Qt::AlignVCenter,
               p.fontMetrics().elidedText(detail, Qt::ElideMiddle, width() - 42));
    p.setPen(QPen(t.ink_secondary, 1.5, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(width() - 21, 27), QPointF(width() - 17, 31));
    p.drawLine(QPointF(width() - 17, 31), QPointF(width() - 21, 35));
  }

private:
  QString title, detail;
};

class ExamplesTile final : public QPushButton
{
public:
  ExamplesTile(const QString        &path,
               int                   count,
               std::function<void()> activate,
               QWidget              *parent)
      : QPushButton(parent), preview(path), count(count)
  {
    setObjectName("selectorExamplesTile");
    setFixedHeight(164);
    setCursor(Qt::PointingHandCursor);
    setAccessibleName(QStringLiteral("Open Examples"));
    setAutoDefault(false);
    connect(this, &QPushButton::clicked, this, std::move(activate));
  }

protected:
  void paintEvent(QPaintEvent *) override
  {
    const auto t = selector_theme();
    QPainter   p(this);
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    const QRectF box = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath clip;
    clip.addRoundedRect(box,
                        t.metrics.section_card_radius,
                        t.metrics.section_card_radius);
    p.save();
    p.setClipPath(clip);
    p.fillRect(box, underMouse() ? t.field_hover : t.bar);
    draw_preview(p, preview, QRectF(0, 0, width(), 112));
    p.restore();
    p.setPen(QPen(hasFocus() || underMouse() ? t.accent : t.field_border, 1));
    p.setBrush(Qt::NoBrush);
    p.drawPath(clip);
    p.setFont(meta::qt::ui_font(14));
    p.setPen(t.ink_primary);
    p.drawText(QRect(14, 119, width() - 28, 24), Qt::AlignVCenter, "Open Examples");
    p.setFont(meta::qt::ui_font(12));
    p.setPen(t.ink_secondary);
    p.drawText(QRect(14, 141, width() - 28, 20),
               Qt::AlignVCenter,
               QStringLiteral("%1 starting points to explore").arg(count));
  }

private:
  QPixmap preview;
  int     count;
};

struct ProjectEntry
{
  QString path, title, subtitle, preview;
  bool    project = false;
};
} // namespace

class ProjectCard final : public QPushButton
{
public:
  ProjectCard(const ProjectEntry &entry, QWidget *parent)
      : QPushButton(parent), entry(entry), preview(entry.preview)
  {
    setObjectName("selectorProjectCard");
    setMinimumWidth(210);
    setFixedHeight(188);
    setCursor(Qt::PointingHandCursor);
    setAccessibleName(entry.title);
    setToolTip(entry.title);
    setAutoDefault(false);
    connect(this,
            &QPushButton::clicked,
            this,
            [this]()
            {
              if (select_callback)
                select_callback(this);
            });
  }
  const QString &file_path() const { return entry.path; }
  bool           is_project() const { return entry.project; }
  void           set_selected(bool value)
  {
    selected = value;
    update();
  }
  void set_select_callback(std::function<void(ProjectCard *)> fn)
  {
    select_callback = std::move(fn);
  }
  void set_activate_callback(std::function<void(ProjectCard *)> fn)
  {
    activate_callback = std::move(fn);
  }

protected:
  void keyPressEvent(QKeyEvent *event) override
  {
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
    {
      if (activate_callback)
        activate_callback(this);
      event->accept();
      return;
    }
    QPushButton::keyPressEvent(event);
  }
  void mouseDoubleClickEvent(QMouseEvent *event) override
  {
    if (event->button() == Qt::LeftButton && activate_callback)
      activate_callback(this);
    else
      QPushButton::mouseDoubleClickEvent(event);
  }
  void paintEvent(QPaintEvent *) override
  {
    const auto t = selector_theme();
    QPainter   p(this);
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    const QRectF box = QRectF(rect()).adjusted(1, 1, -1, -1);
    QPainterPath clip;
    clip.addRoundedRect(box,
                        t.metrics.section_card_radius,
                        t.metrics.section_card_radius);
    p.save();
    p.setClipPath(clip);
    p.fillRect(box,
               selected       ? blend_color(t.bar, t.accent, 0.12)
               : underMouse() ? t.field_hover
                              : t.bar);
    const QRectF image(1, 1, width() - 2, height() - 49);
    if (preview.isNull())
    {
      p.setFont(meta::qt::ui_font(14));
      p.setPen(t.ink_secondary);
      p.drawText(image, Qt::AlignCenter, "Preview unavailable");
    }
    else
      draw_preview(p, preview, image);
    p.restore();
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(selected || hasFocus() ? t.accent : t.field_border,
                  selected || hasFocus() ? 2 : 1));
    p.drawPath(clip);
    p.setFont(meta::qt::ui_font(14, selected));
    p.setPen(t.ink_primary);
    p.drawText(QRect(14, height() - 43, width() - 28, 34),
               Qt::AlignVCenter,
               p.fontMetrics().elidedText(entry.title, Qt::ElideRight, width() - 28));
  }

private:
  ProjectEntry                       entry;
  QPixmap                            preview;
  bool                               selected = false;
  std::function<void(ProjectCard *)> select_callback, activate_callback;
};

ExampleSelectorDialog::ExampleSelectorDialog(const QString &examples_path,
                                             QWidget       *parent)
    : QDialog(parent), examples_path(examples_path)
{
  Logger::log()->trace("ExampleSelectorDialog::ExampleSelectorDialog");

  this->setObjectName("exampleSelector");
  this->setWindowTitle("Hesiod Project Browser");
  this->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
  this->setModal(true);
  this->setAttribute(Qt::WA_TranslucentBackground);
  this->setFont(meta::qt::ui_font(14));

  QScreen    *screen = parent ? parent->screen() : QGuiApplication::primaryScreen();
  const QRect available = screen ? screen->availableGeometry() : QRect(0, 0, 900, 700);
  const int   max_width = std::max(460, available.width() - 24);
  const int   max_height = std::max(400, available.height() - 24);
  const int   min_width = std::min(560, max_width);
  const int   min_height = std::min(450, max_height);
  const int   width = std::clamp(qRound(available.width() * 0.82),
                               min_width,
                               std::min(900, max_width));
  const int   height = std::clamp(qRound(available.height() * 0.72),
                                min_height,
                                std::min(740, max_height));
  this->resize(width, height);
  this->setMinimumSize(std::min(680, width), std::min(600, height));
  this->move(available.center() - this->rect().center());

  auto *root_layout = new QVBoxLayout(this);
  root_layout->setContentsMargins(1, 1, 1, 1);
  root_layout->setSpacing(0);

  auto *title_bar = new DialogTitleBar(this);
  auto *title_layout = new QHBoxLayout(title_bar);
  title_layout->setContentsMargins(14, 0, 8, 0);
  title_layout->setSpacing(9);

  auto *identity = new QLabel(title_bar);
  identity->setFixedSize(24, 24);
  identity->setAlignment(Qt::AlignCenter);
  identity->setPixmap(QIcon(QString::fromStdString(HSD_CTX.app_settings.global.icon_path))
                          .pixmap(QSize(24, 24), this->devicePixelRatioF()));
  identity->setAccessibleName(QStringLiteral("Hesiod"));
  identity->setObjectName("selectorIdentity");
  identity->setAttribute(Qt::WA_TransparentForMouseEvents);
  title_layout->addWidget(identity);

  auto *title_divider = new QFrame(title_bar);
  title_divider->setObjectName("selectorTitleDivider");
  title_divider->setFrameShape(QFrame::VLine);
  title_divider->setFixedSize(1, 18);
  title_layout->addWidget(title_divider);

  auto *window_label = new QLabel(QStringLiteral("Welcome to Hesiod 0.6.0"), title_bar);
  window_label->setObjectName("selectorWindowLabel");
  window_label->setAttribute(Qt::WA_TransparentForMouseEvents);
  title_layout->addWidget(window_label);
  title_layout->addStretch();

  auto *close_button = new QPushButton(QStringLiteral("\u00d7"), title_bar);
  close_button->setObjectName("selectorClose");
  close_button->setToolTip(QStringLiteral("Close and start a blank project"));
  title_layout->addWidget(close_button);
  root_layout->addWidget(title_bar);

  this->pages = new QStackedWidget(this);
  this->pages->setObjectName("selectorPages");

  // Welcome page: intentionally sparse, with the action stack and recents on
  // the right and a quiet Hesiod watermark anchoring the left half.
  this->welcome_page = new WelcomeCanvas(this->pages);
  auto *welcome_layout = new QHBoxLayout(this->welcome_page);
  welcome_layout->setContentsMargins(38, 34, 34, 26);
  welcome_layout->setSpacing(28);

  auto *brand_layout = new QVBoxLayout();
  brand_layout->setContentsMargins(0, 0, 0, 0);
  brand_layout->setSpacing(5);
  brand_layout->addSpacing(44);
  auto *brand = new QLabel(QStringLiteral("HESIOD"), this->welcome_page);
  brand->setObjectName("selectorBrand");
  brand_layout->addWidget(brand);
  auto *brand_subtitle = new QLabel(QStringLiteral("Procedural Terrain Design"),
                                    this->welcome_page);
  brand_subtitle->setObjectName("selectorBrandSubtitle");
  brand_layout->addWidget(brand_subtitle);
  brand_layout->addStretch(4);
  auto *build_label = new QLabel(QStringLiteral("Hesiod 0.6.0"), this->welcome_page);
  build_label->setObjectName("selectorBuild");
  brand_layout->addWidget(build_label);
  welcome_layout->addLayout(brand_layout, 1);

  auto *action_panel = new QWidget(this->welcome_page);
  action_panel->setObjectName("selectorActionPanel");
  action_panel->setFixedWidth(310);
  auto *action_layout = new QVBoxLayout(action_panel);
  action_layout->setContentsMargins(0, 0, 0, 0);
  action_layout->setSpacing(8);

  auto *new_project_button = new QPushButton(QStringLiteral("New Project"), action_panel);
  new_project_button->setObjectName("selectorWelcomePrimary");
  action_layout->addWidget(new_project_button);

  auto *open_project_button = new QPushButton(QStringLiteral("Open From File..."),
                                              action_panel);
  open_project_button->setObjectName("selectorWelcomeSecondary");
  action_layout->addWidget(open_project_button);

  this->recent_search = new QLineEdit(action_panel);
  this->recent_search->setObjectName("selectorRecentSearch");
  this->recent_search->setPlaceholderText(QStringLiteral("Search recent projects..."));
  this->recent_search->setClearButtonEnabled(true);
  this->recent_search->addAction(search_icon(), QLineEdit::LeadingPosition);
  action_layout->addSpacing(5);
  action_layout->addWidget(this->recent_search);

  auto *recent_label = new QLabel(QStringLiteral("Recent projects"), action_panel);
  recent_label->setObjectName("selectorSectionLabel");
  action_layout->addWidget(recent_label);

  this->recent_host = new QWidget(action_panel);
  this->recent_host->setObjectName("selectorRecentHost");
  this->recent_layout = new QVBoxLayout(this->recent_host);
  this->recent_layout->setContentsMargins(0, 0, 0, 0);
  this->recent_layout->setSpacing(6);
  action_layout->addWidget(this->recent_host);
  action_layout->addStretch(1);

  QDir              examples_dir(this->examples_path);
  const QStringList example_files = examples_dir.entryList({"*.hsd"}, QDir::Files);
  QString           tile_preview;
  if (example_files.contains(QStringLiteral("island.hsd")))
    tile_preview = preview_for_project(QFileInfo(examples_dir.filePath("island.hsd")));
  else if (!example_files.isEmpty())
    tile_preview = preview_for_project(
        QFileInfo(examples_dir.filePath(example_files.first())));

  auto *examples_tile = new ExamplesTile(
      tile_preview,
      example_files.size(),
      [this]() { this->show_examples(); },
      action_panel);
  action_layout->addWidget(examples_tile);
  welcome_layout->addWidget(action_panel);
  this->pages->addWidget(this->welcome_page);

  // Examples page: a focused secondary view, reached from the single welcome
  // tile rather than presenting a wall of thumbnails at startup.
  this->examples_page = new QWidget(this->pages);
  this->examples_page->setObjectName("selectorExamplesPage");
  auto *examples_layout = new QVBoxLayout(this->examples_page);
  examples_layout->setContentsMargins(24, 20, 24, 20);
  examples_layout->setSpacing(10);

  auto *examples_header = new QHBoxLayout();
  auto *back_button = new QPushButton(QStringLiteral("Back"), this->examples_page);
  back_button->setObjectName("selectorBackButton");
  QObject::connect(back_button,
                   &QPushButton::clicked,
                   this,
                   &ExampleSelectorDialog::show_welcome);
  examples_header->addWidget(back_button);
  examples_header->addSpacing(8);
  auto *examples_heading = new QLabel(QStringLiteral("Select an Example Project"),
                                      this->examples_page);
  examples_heading->setObjectName("selectorHeading");
  examples_header->addWidget(examples_heading);
  examples_header->addStretch();
  this->result_count = new QLabel(this->examples_page);
  this->result_count->setObjectName("selectorCount");
  this->result_count->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  examples_header->addWidget(this->result_count);
  examples_layout->addLayout(examples_header);

  this->search = new QLineEdit(this->examples_page);
  this->search->setObjectName("selectorSearch");
  this->search->setPlaceholderText(QStringLiteral("Search examples..."));
  this->search->setClearButtonEnabled(true);
  this->search->addAction(search_icon(), QLineEdit::LeadingPosition);
  examples_layout->addWidget(this->search);

  this->scroll_area = new SmoothScrollArea(this->examples_page);
  this->scroll_area->setObjectName("selectorScroll");
  this->scroll_area->setWidgetResizable(true);
  this->scroll_area->setFrameShape(QFrame::NoFrame);
  this->scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  this->cards_host = new QWidget(this->scroll_area);
  this->cards_host->setObjectName("selectorCardsHost");
  this->cards_layout = new QGridLayout(this->cards_host);
  this->cards_layout->setContentsMargins(1, 1, 5, 1);
  this->cards_layout->setHorizontalSpacing(14);
  this->cards_layout->setVerticalSpacing(12);
  this->cards_layout->setAlignment(Qt::AlignTop);
  this->empty_state = new QLabel(this->cards_host);
  this->empty_state->setObjectName("selectorEmpty");
  this->empty_state->setAlignment(Qt::AlignCenter);
  this->empty_state->setWordWrap(true);
  this->empty_state->setMinimumHeight(190);
  this->scroll_area->setWidget(this->cards_host);
  examples_layout->addWidget(this->scroll_area, 1);
  this->pages->addWidget(this->examples_page);
  root_layout->addWidget(this->pages, 1);

  auto *footer = new QWidget(this);
  footer->setObjectName("selectorFooter");
  auto *footer_layout = new QHBoxLayout(footer);
  footer_layout->setContentsMargins(16, 10, 16, 10);
  footer_layout->setSpacing(8);

  this->startup_check = new QCheckBox(QStringLiteral("Show at startup"), footer);
  this->startup_check->setObjectName("selectorStartupCheck");
  this->startup_check->setChecked(
      HSD_CTX.app_settings.interface.enable_example_selector_at_startup);
  footer_layout->addWidget(this->startup_check);
  footer_layout->addStretch();

  auto *file_button = new QPushButton(QStringLiteral("Open From File..."), footer);
  file_button->setObjectName("selectorFileButton");
  footer_layout->addWidget(file_button);
  QObject::connect(file_button,
                   &QPushButton::clicked,
                   this,
                   &ExampleSelectorDialog::open_project);

  this->secondary_button = new QPushButton(QStringLiteral("New Project"), footer);
  this->secondary_button->setObjectName("selectorSecondaryButton");
  footer_layout->addWidget(this->secondary_button);

  this->open_button = new QPushButton(QStringLiteral("Open Example"), footer);
  this->open_button->setObjectName("selectorPrimaryButton");
  this->open_button->setAutoDefault(false);
  footer_layout->addWidget(this->open_button);
  root_layout->addWidget(footer);

  this->apply_theme();

  QObject::connect(close_button,
                   &QPushButton::clicked,
                   this,
                   &ExampleSelectorDialog::on_reject);
  QObject::connect(new_project_button,
                   &QPushButton::clicked,
                   this,
                   &ExampleSelectorDialog::start_new_project);
  QObject::connect(open_project_button,
                   &QPushButton::clicked,
                   this,
                   &ExampleSelectorDialog::open_project);
  QObject::connect(this->open_button,
                   &QPushButton::clicked,
                   this,
                   &ExampleSelectorDialog::on_accept);
  QObject::connect(this->secondary_button,
                   &QPushButton::clicked,
                   this,
                   [this]() { this->start_new_project(); });
  QObject::connect(this->search,
                   &QLineEdit::textChanged,
                   this,
                   [this]() { this->rebuild_cards(); });
  QObject::connect(this->recent_search,
                   &QLineEdit::textChanged,
                   this,
                   [this]() { this->rebuild_recent_projects(); });
  QObject::connect(
      this->startup_check,
      &QCheckBox::toggled,
      this,
      [](bool checked)
      {
        HSD_CTX.app_settings.interface.enable_example_selector_at_startup = checked;
        HSD_CTX.save_settings();
      });

  this->show_welcome();
}

void ExampleSelectorDialog::apply_theme()
{
  const auto t = selector_theme();
  QString    css = QStringLiteral(R"(
    QDialog#exampleSelector { background: transparent; }
    QWidget { font-family: "UI_FONT"; font-size: 14px; }
    QLabel { color: INK; background: transparent; border: none; }
    QWidget#selectorTitleBar { background: BAR; border: none;
      border-top-left-radius: CARD_RADIUSpx; border-top-right-radius: CARD_RADIUSpx; }
    QLabel#selectorIdentity { color: ACCENT; }
    QFrame#selectorTitleDivider { border: none; background: transparent; }
    QLabel#selectorWindowLabel { color: SECONDARY; }
    QPushButton#selectorClose { background: transparent; border: none;
      min-width: 32px; min-height: 32px; padding: 0; font-size: 23px; }
    QPushButton#selectorClose:hover { background: HOVER; }
    QStackedWidget#selectorPages, QWidget#selectorExamplesPage { background: PAGE; border: none; }
    QWidget#selectorActionPanel, QWidget#selectorRecentHost { background: transparent; }
    QLabel#selectorBrand { font-size: 32px; font-weight: 600; color: INK; }
    QLabel#selectorBrandSubtitle { color: SECONDARY; font-size: 14px; }
    QLabel#selectorBuild { color: SECONDARY; font-size: 12px; }
    QLabel#selectorSectionLabel { color: SECONDARY; padding-top: 8px; }
    QLabel#selectorHeading { font-size: 20px; font-weight: 600; }
    QLabel#selectorCount, QLabel#selectorRecentEmpty { color: SECONDARY; font-size: 12px; }
    QLabel#selectorEmpty { color: SECONDARY; padding: 24px; }
    QLineEdit { background: FIELD; color: INK; border: 1px solid BORDER;
      border-radius: RADIUSpx; min-height: 40px; padding: 0 12px;
      selection-background-color: ACCENT; }
    QLineEdit:hover { border-color: SECONDARY; }
    QLineEdit:focus { border-color: ACCENT; }
    QPushButton { color: INK; background: BAR; border: 1px solid BORDER;
      border-radius: RADIUSpx; min-height: 40px; padding: 0 16px;
      font-weight: normal; }
    QPushButton#selectorRecentRow { min-height: 62px; max-height: 62px; padding: 0; border: none; }
    QPushButton#selectorExamplesTile { min-height: 164px; max-height: 164px; padding: 0; border: none; }
    QPushButton#selectorProjectCard { min-height: 188px; max-height: 188px; padding: 0; border: none; }
    QPushButton:hover { background: HOVER; border-color: SECONDARY; }
    QPushButton:focus { border-color: ACCENT; }
    QPushButton:pressed { background: FIELD; }
    QPushButton#selectorWelcomePrimary, QPushButton#selectorPrimaryButton {
      background: SELECTED; border-color: ACCENT; }
    QPushButton#selectorPrimaryButton:disabled { color: DISABLED; border-color: BORDER; background: BAR; }
    QPushButton#selectorBackButton { min-height: 32px; padding: 0 12px; }
    QScrollArea#selectorScroll, QWidget#selectorCardsHost { background: transparent; border: none; }
    QScrollArea#selectorScroll QScrollBar:vertical { width: 8px; margin: 0; background: BAR; border: none; }
    QScrollArea#selectorScroll QScrollBar::handle:vertical { min-height: 32px; background: ACCENT; border-radius: 4px; }
    QScrollArea#selectorScroll QScrollBar::add-line:vertical,
    QScrollArea#selectorScroll QScrollBar::sub-line:vertical { height: 0; border: none; }
    QScrollArea#selectorScroll QScrollBar::add-page:vertical,
    QScrollArea#selectorScroll QScrollBar::sub-page:vertical { background: transparent; }
    QWidget#selectorFooter { background: PAGE; border: none;
      border-bottom-left-radius: CARD_RADIUSpx; border-bottom-right-radius: CARD_RADIUSpx; }
    QCheckBox#selectorStartupCheck { color: SECONDARY; background: transparent;
      border: none; spacing: 9px; }
    QCheckBox#selectorStartupCheck::indicator { width: 18px; height: 18px;
      border: 1px solid BORDER; border-radius: 4px; background: FIELD; }
    QCheckBox#selectorStartupCheck::indicator:checked { background: SELECTED;
      border-color: ACCENT; image: url(CHECK_ICON); }
  )");
  css.replace("UI_FONT", meta::qt::ui_font(14).family());
  css.replace("CARD_RADIUS", QString::number(t.metrics.section_card_radius));
  css.replace("RADIUS", QString::number(std::max(6, t.metrics.radius)));
  const std::pair<const char *, QColor> colors[] = {
      {"PAGE", t.page},
      {"BAR", t.bar},
      {"INK", t.ink_primary},
      {"SECONDARY", t.ink_secondary},
      {"ACCENT", t.accent},
      {"FIELD", t.field},
      {"BORDER", t.field_border},
      {"HOVER", t.field_hover},
      {"DISABLED", t.ink_locked},
      {"SELECTED", blend_color(t.bar, t.accent, 0.14)}};
  for (const auto &[key, value] : colors)
    css.replace(key, value.name());
  const auto &icons = HSD_CTX.app_settings.icons.icon_paths;
  const auto  check = icons.find("check");
  css.replace("CHECK_ICON",
              check != icons.end() ? QString::fromStdString(check->second)
                                   : QStringLiteral("data/icons/pp_check.svg"));
  this->setStyleSheet(css);
  this->update();
}

void ExampleSelectorDialog::paintEvent(QPaintEvent *)
{
  const auto t = selector_theme();
  QPainter   p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(QPen(t.field_border, 1));
  p.setBrush(t.page);
  p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5),
                    t.metrics.section_card_radius,
                    t.metrics.section_card_radius);
}

void ExampleSelectorDialog::changeEvent(QEvent *event)
{
  QDialog::changeEvent(event);
  if (event->type() == QEvent::ApplicationPaletteChange && this->pages)
    this->apply_theme();
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

void ExampleSelectorDialog::accept_card(ProjectCard *card)
{
  this->select_card(card);
  this->on_accept();
}

void ExampleSelectorDialog::clear_cards()
{
  while (QLayoutItem *item = this->cards_layout->takeAt(0))
  {
    QWidget *widget = item->widget();
    if (widget && widget != this->empty_state)
      widget->deleteLater();
    delete item;
  }

  this->cards.clear();
  this->selected_card = nullptr;
  this->selected_filename.clear();
  this->selected_project = false;
  this->card_columns = 0;
  this->open_button->setEnabled(false);
}

void ExampleSelectorDialog::keyPressEvent(QKeyEvent *event)
{
  if (event->key() == Qt::Key_Escape)
  {
    if (this->pages->currentWidget() == this->examples_page)
      this->show_welcome();
    else
      this->on_reject();
    return;
  }

  if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) &&
      this->pages->currentWidget() == this->examples_page && this->selected_card)
  {
    this->on_accept();
    return;
  }

  QDialog::keyPressEvent(event);
}

void ExampleSelectorDialog::on_accept()
{
  if (!this->selected_card || this->selected_filename.isEmpty())
    return;
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

void ExampleSelectorDialog::rebuild_recent_projects()
{
  while (QLayoutItem *item = this->recent_layout->takeAt(0))
  {
    if (QWidget *widget = item->widget())
      widget->deleteLater();
    delete item;
  }

  const QString query = this->recent_search->text().trimmed();
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

    const QString title = pretty_project_name(info);
    if (!query.isEmpty() && !title.contains(query, Qt::CaseInsensitive) &&
        !info.absoluteFilePath().contains(query, Qt::CaseInsensitive))
      continue;

    const QString detail = QStringLiteral("%1  \u00b7  %2")
                               .arg(info.lastModified().toString("dd MMM yyyy"))
                               .arg(QDir::toNativeSeparators(info.absolutePath()));
    const QString path = info.absoluteFilePath();
    auto         *row = new RecentProjectRow(
        title,
        detail,
        [this, path]() { this->accept_path(path, true); },
        this->recent_host);
    row->setToolTip(path);
    this->recent_layout->addWidget(row);

    if (++shown >= 3)
      break;
  }

  if (shown == 0)
  {
    auto *empty = new QLabel(query.isEmpty() ? QStringLiteral("No recent projects yet")
                                             : QStringLiteral("No matching projects"),
                             this->recent_host);
    empty->setObjectName("selectorRecentEmpty");
    empty->setAlignment(Qt::AlignCenter);
    empty->setFixedHeight(54);
    this->recent_layout->addWidget(empty);
  }
}

void ExampleSelectorDialog::rebuild_cards()
{
  this->clear_cards();
  this->empty_state->hide();

  const QString         query = this->search->text().trimmed();
  QVector<ProjectEntry> entries;

  QDir              dir(this->examples_path);
  const QStringList files = dir.entryList({"*.hsd"},
                                          QDir::Files | QDir::Readable,
                                          QDir::Name | QDir::IgnoreCase);
  for (const QString &file : files)
  {
    const QFileInfo info(dir.filePath(file));
    const QString   title = pretty_project_name(info);
    if (!query.isEmpty() && !title.contains(query, Qt::CaseInsensitive) &&
        !file.contains(query, Qt::CaseInsensitive))
      continue;

    entries.push_back({info.absoluteFilePath(),
                       title,
                       QStringLiteral("CURATED  ·  %1").arg(file_size_label(info.size())),
                       preview_for_project(info),
                       false});
  }

  for (const ProjectEntry &entry : entries)
  {
    auto *card = new ProjectCard(entry, this->cards_host);
    card->set_select_callback([this](ProjectCard *selected)
                              { this->select_card(selected); });
    card->set_activate_callback([this](ProjectCard *selected)
                                { this->accept_card(selected); });
    this->cards.push_back(card);
  }

  this->result_count->setText(QStringLiteral("%1 examples").arg(entries.size()));

  if (this->cards.isEmpty())
  {
    this->empty_state->setText(
        QStringLiteral("No examples found\n\nTry a different name."));
    this->empty_state->show();
    this->cards_layout->addWidget(this->empty_state, 0, 0, 1, 2);
    return;
  }

  this->relayout_cards();
  this->select_card(this->cards.first());
}

void ExampleSelectorDialog::relayout_cards()
{
  if (this->cards.isEmpty() || !this->scroll_area)
    return;

  const int columns = std::clamp(this->scroll_area->viewport()->width() / 220, 1, 3);
  if (columns == this->card_columns && this->cards_layout->count() > 0)
    return;
  this->card_columns = columns;

  while (QLayoutItem *item = this->cards_layout->takeAt(0))
    delete item;

  for (int column = 0; column < 3; ++column)
    this->cards_layout->setColumnStretch(column, column < columns ? 1 : 0);
  for (int index = 0; index < this->cards.size(); ++index)
  {
    ProjectCard *card = this->cards[index];
    card->setFixedHeight(188);
    this->cards_layout->addWidget(card, index / columns, index % columns);
  }
  this->cards_host->updateGeometry();
}

void ExampleSelectorDialog::resizeEvent(QResizeEvent *event)
{
  QDialog::resizeEvent(event);
  this->relayout_cards();
}

void ExampleSelectorDialog::select_card(ProjectCard *card)
{
  if (!card)
    return;

  this->selected_card = card;
  for (ProjectCard *candidate : this->cards)
    candidate->set_selected(candidate == card);

  this->selected_filename = card->file_path();
  this->selected_project = card->is_project();
  this->open_button->setEnabled(true);
  this->open_button->setText(this->selected_project ? QStringLiteral("OPEN PROJECT")
                                                    : QStringLiteral("Open Example"));
}

ExampleSelectorDialog::Outcome ExampleSelectorDialog::outcome() const
{
  return this->result;
}

void ExampleSelectorDialog::start_new_project()
{
  // Accepted, not rejected. A rejected dialog means the user closed it, and
  // the caller treats that as "do not continue"; asking for a new project is a
  // choice and has to be reported as one, or from the menu bar the window just
  // closes and the open project stays put.
  this->selected_filename.clear();
  this->selected_project = false;
  this->result = Outcome::NewProject;
  this->accept();
}

QString ExampleSelectorDialog::selected_file() const { return this->selected_filename; }

bool ExampleSelectorDialog::selected_is_project() const { return this->selected_project; }

void ExampleSelectorDialog::show_examples()
{
  this->pages->setCurrentWidget(this->examples_page);
  this->open_button->setDefault(true);
  this->secondary_button->setText(QStringLiteral("New Project"));
  this->secondary_button->show();
  this->findChild<QPushButton *>("selectorFileButton")->show();
  this->open_button->show();
  this->rebuild_cards();
  this->search->setFocus(Qt::OtherFocusReason);

  // The stacked page receives its final viewport geometry after the page
  // switch. Relayout once that happens so a wide view starts with the appropriate number
  // of columns.
  QTimer::singleShot(0,
                     this,
                     [this]()
                     {
                       this->card_columns = 0;
                       this->relayout_cards();
                     });
}

void ExampleSelectorDialog::show_welcome()
{
  this->pages->setCurrentWidget(this->welcome_page);
  this->open_button->setDefault(false);
  this->secondary_button->setText(QStringLiteral("New Project"));
  this->secondary_button->hide();
  this->findChild<QPushButton *>("selectorFileButton")->hide();
  this->open_button->hide();
  this->rebuild_recent_projects();
  this->findChild<QPushButton *>("selectorWelcomePrimary")
      ->setFocus(Qt::OtherFocusReason);
}

} // namespace hesiod
