/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#include <algorithm>

#include <QClipboard>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/error_dialog.hpp"
#include "hesiod/gui/widgets/gui_utils.hpp"
#include "hesiod/logger.hpp"

namespace hesiod
{

namespace
{

QString category_tag(ErrorCategory category)
{
  switch (category)
  {
  case ErrorCategory::NodeCreation:
    return "Node";
  case ErrorCategory::LinkCreation:
    return "Link";
  case ErrorCategory::IO:
    return "File";
  case ErrorCategory::Deserialization:
    return "Load";
  case ErrorCategory::GraphExecution:
    return "Graph";
  case ErrorCategory::Bake:
    return "Bake";
  case ErrorCategory::Export:
    return "Export";
  default:
    return "Issue";
  }
}

} // namespace

ErrorDialog::ErrorDialog(const QString            &title,
                         const QString            &message,
                         const std::vector<Error> &errors,
                         bool                      show_cancel_button,
                         QWidget                  *parent)
    : MessageDialog(parent, MessageDialog::Kind::Warning, title, message)
{
  std::vector<Item> items;
  for (const Error &error : errors)
  {
    const QString text = QString::fromStdString(error.get_message());
    items.push_back({category_tag(error.get_category()), text});
    this->raw_text += "- " + text + "\n";
  }

  this->setup_ui(items, show_cancel_button);
}

ErrorDialog::ErrorDialog(const QString &title,
                         const QString &message,
                         const QString &error_text,
                         bool           show_cancel_button,
                         QWidget       *parent)
    : MessageDialog(parent, MessageDialog::Kind::Warning, title, message)
{
  std::vector<Item> items;
  for (const QString &line : error_text.split('\n', Qt::SkipEmptyParts))
  {
    QString text = line.trimmed();
    if (text.startsWith(QChar(0x2022)) || text.startsWith('-'))
      text = text.mid(1).trimmed();
    items.push_back({QString(), text});
  }
  this->raw_text = error_text;

  this->setup_ui(items, show_cancel_button);
}

void ErrorDialog::setup_ui(const std::vector<Item> &items, bool show_cancel_button)
{
  Logger::log()->trace("ErrorDialog::setup_ui");

  const auto &colors = HSD_CTX.app_settings.colors;

  this->set_card_width(580);

  // --- problem list: one row per problem, "what" over "why"
  auto *scroll = new QScrollArea(this);
  scroll->setObjectName("problemScroll");
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  auto *list = new QWidget();
  list->setObjectName("problemList");
  auto *list_layout = new QVBoxLayout(list);
  list_layout->setContentsMargins(0, 0, 0, 0);
  list_layout->setSpacing(0);

  for (size_t i = 0; i < items.size(); ++i)
  {
    const Item &item = items[i];

    auto *row = new QWidget(list);
    row->setObjectName(i + 1 < items.size() ? "problemRow" : "problemRowLast");
    row->setAttribute(Qt::WA_StyledBackground);
    auto *row_layout = new QHBoxLayout(row);
    row_layout->setContentsMargins(12, 9, 12, 9);
    row_layout->setSpacing(10);

    auto *tag = new QLabel(item.tag.isEmpty() ? "Issue" : item.tag, row);
    tag->setObjectName("problemTag");
    tag->setAlignment(Qt::AlignCenter);
    tag->setFixedWidth(46);
    row_layout->addWidget(tag, 0, Qt::AlignTop);

    // "Failed to create link a => b: Source node not found: 28"
    //  -> what: "Failed to create link a => b", why: "Source node not found: 28"
    QString         what = item.text;
    QString         why;
    const qsizetype split = item.text.indexOf(": ");
    if (split > 0)
    {
      what = item.text.left(split);
      why = item.text.mid(split + 2);
    }

    auto *text = new QVBoxLayout();
    text->setSpacing(2);
    auto *what_label = new QLabel(what, row);
    what_label->setObjectName("problemWhat");
    what_label->setWordWrap(true);
    what_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    text->addWidget(what_label);
    if (!why.isEmpty())
    {
      auto *why_label = new QLabel(why, row);
      why_label->setObjectName("problemWhy");
      why_label->setWordWrap(true);
      why_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
      text->addWidget(why_label);
    }
    row_layout->addLayout(text, 1);

    list_layout->addWidget(row);
  }
  list_layout->addStretch(1);
  scroll->setWidget(list);

  // tall enough for a handful of rows, capped so a long list scrolls
  const int rows_shown = std::min<int>(int(items.size()), 6);
  scroll->setFixedHeight(std::max(60, rows_shown * 52 + 2));

  auto *frame = new QFrame(this);
  frame->setObjectName("problemFrame");
  auto *frame_layout = new QVBoxLayout(frame);
  frame_layout->setContentsMargins(1, 1, 1, 1);
  frame_layout->addWidget(scroll);
  this->body()->addWidget(frame);

  // --- buttons
  QPushButton *copy = this->add_button("Copy details",
                                       MessageDialog::Role::Secondary,
                                       false,
                                       false,
                                       false);
  copy->setToolTip("Copy the full list to the clipboard");
  QObject::connect(
      copy,
      &QPushButton::clicked,
      this,
      [this, copy]()
      {
        QGuiApplication::clipboard()->setText(this->raw_text);
        copy->setText("Copied");
        QTimer::singleShot(1500, copy, [copy]() { copy->setText("Copy details"); });
      });

  if (show_cancel_button)
  {
    this->add_button("Cancel", MessageDialog::Role::Secondary, false, true);
    this->add_button("Continue", MessageDialog::Role::Primary, true);
  }
  else
  {
    this->add_button("OK", MessageDialog::Role::Primary, true);
  }

  // --- extra styling for the list
  QString                               css = R"(
    QDialog#hsdMessage QFrame#problemFrame {
      background: DEEP; border: 1px solid BORDER; border-radius: 8px; }
    QDialog#hsdMessage QWidget#problemRow { border-bottom: 1px solid BORDER; }
    QDialog#hsdMessage QLabel#problemTag {
      color: WARN; background: WARN_SOFT; border-radius: 4px;
      padding: 1px 0px; font-size: 10px; font-weight: 600; }
    QDialog#hsdMessage QLabel#problemWhat { color: INK; font-size: 12px; }
    QDialog#hsdMessage QLabel#problemWhy { color: DIM; font-size: 12px; }
    QDialog#hsdMessage QScrollBar:vertical {
      width: 10px; background: transparent; margin: 4px 2px 4px 0px; border: none; }
    QDialog#hsdMessage QScrollBar::handle:vertical {
      background: BORDER; border-radius: 4px; min-height: 30px; }
    QDialog#hsdMessage QScrollBar::add-line:vertical,
    QDialog#hsdMessage QScrollBar::sub-line:vertical { height: 0px; border: none; }
    QDialog#hsdMessage QScrollBar::add-page:vertical,
    QDialog#hsdMessage QScrollBar::sub-page:vertical { background: transparent; }
  )";
  const QColor                          warn("#d9a441");
  const QColor                          deep = colors.bg_deep;
  const std::pair<const char *, QColor> tokens[] = {
      {"WARN_SOFT", mix_colors(deep, warn, 0.16)},
      {"WARN", warn},
      {"DEEP", deep},
      {"BORDER", panel_border_color()},
      {"DIM", mix_colors(colors.bg_primary, colors.text_primary, 0.66)},
      {"INK", colors.text_primary}};
  for (const auto &[key, value] : tokens)
    css.replace(key, value.name());

  this->setStyleSheet(this->styleSheet() + css);
}

} // namespace hesiod
