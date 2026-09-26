/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <any>
#include <format>
#include <string>

#include <QDir>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "meta/core/attribute_container.hpp"
#include "meta/metadata/keys.hpp"
#include "meta/presets/choice.hpp"
#include "meta/presets/numeric.hpp"
#include "meta_qt/container_widget.hpp"
#include "meta_qt/ui/theme.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/graph_config_widgets/bake_config_dialog.hpp"
#include "hesiod/gui/widgets/properties_panel_design.hpp"

namespace hesiod
{

namespace
{

const meta::qt::Theme &bake_theme()
{
  static const meta::qt::Theme theme = []()
  {
    meta::qt::Theme t = *properties_panel_design().theme;
    t.group_accents = {{"Output", QColor("#cfa143")},
                       {"Options", QColor("#7d9cc0")},
                       {"Large Graphs", QColor("#3aa899")}};
    return t;
  }();
  return theme;
}

void tag(meta::AbstractAttribute &a, const std::string &category, const std::string &tip)
{
  a.metadata().try_add(std::string(meta::keys::ui::category), std::string(category));
  if (!tip.empty())
    a.metadata().try_add(std::string(meta::keys::ui::tooltip), std::string(tip));
}

std::vector<std::pair<int, std::string>> power_of_two_items(int from, int to)
{
  std::vector<std::pair<int, std::string>> items;
  for (int size = from; size <= to; size *= 2)
    items.push_back({size, std::to_string(size)});
  return items;
}

// value a setting starts from in a fresh project, for the modified state
std::any default_for(const std::string &key)
{
  const BakeConfig d;
  if (key == "resolution")
    return d.resolution;
  if (key == "nvariants")
    return d.nvariants;
  if (key == "force_distributed")
    return d.force_distributed;
  if (key == "force_auto_export")
    return d.force_auto_export;
  if (key == "rename_export_files")
    return d.rename_export_files;
  if (key == "force_maximum_fbm_octaves")
    return d.force_maximum_fbm_octaves;
  if (key == "min_memory")
    return d.min_memory;
  if (key == "max_tile_resolution")
    return d.max_tile_resolution;
  return {};
}

} // namespace

BakeConfigDialog::BakeConfigDialog(int               max_size,
                                   const BakeConfig &initial_value,
                                   const QString    &default_dir,
                                   QWidget          *parent)
    : MessageDialog(parent,
                    Kind::None,
                    "Bake and Export",
                    "Evaluate the graph at full resolution and write every export node."),
      output(std::make_unique<meta::AttributeContainer>()),
      options(std::make_unique<meta::AttributeContainer>()),
      memory(std::make_unique<meta::AttributeContainer>())
{
  this->set_card_width(520);

  // --- output
  {
    auto &c = *this->output;
    // the sizes a bake is actually made at, up to 16K, labelled with their
    // side and the usual shorthand
    std::vector<std::pair<int, std::string>> items;
    for (int size = 256; size <= std::min(max_size, 16384); size *= 2)
      items.push_back({size,
                       std::format("{0} × {0}{1}",
                                   size,
                                   size >= 1024 ? std::format("  ·  {}K", size / 1024)
                                                : std::string{})});

    // a project baked at a size outside the list keeps it
    int res = std::clamp(initial_value.resolution, 2, std::max(2, max_size));
    if (std::none_of(items.begin(),
                     items.end(),
                     [res](const auto &e) { return e.first == res; }))
      items.push_back({res, std::format("{0} × {0}", res)});
    tag(meta::presets::enum_choice(c, "resolution", "Resolution", items, res),
        "Output",
        "Width of the baked heightmaps, in pixels");
    tag(meta::presets::slider_int(c,
                                  "nvariants",
                                  "Additional Variants",
                                  initial_value.nvariants,
                                  0,
                                  50),
        "Output",
        "");
  }

  // --- options
  {
    auto &c = *this->options;
    tag(meta::presets::toggle_button(c,
                                     "force_distributed",
                                     "Distributed Computation",
                                     initial_value.force_distributed),
        "Options",
        "Force distributed computation");
    tag(meta::presets::toggle_button(c,
                                     "force_auto_export",
                                     "Auto Export Nodes",
                                     initial_value.force_auto_export),
        "Options",
        "Force auto export for export nodes");
    tag(meta::presets::toggle_button(c,
                                     "rename_export_files",
                                     "Prefix File Names",
                                     initial_value.rename_export_files),
        "Options",
        "Add a prefix to the export file names");
    tag(meta::presets::toggle_button(c,
                                     "force_maximum_fbm_octaves",
                                     "Maximum Fbm Octaves",
                                     initial_value.force_maximum_fbm_octaves),
        "Options",
        "Force the maximum number of octaves for Fbm nodes");
  }

  // --- large graphs and memory
  {
    auto &c = *this->memory;
    tag(meta::presets::toggle_button(c,
                                     "min_memory",
                                     "Low Memory Mode",
                                     initial_value.min_memory),
        "Large Graphs",
        "");
    const auto items = power_of_two_items(128, max_size);
    int        tile = initial_value.max_tile_resolution;
    bool       known = false;
    for (const auto &[v, _] : items)
      known |= v == tile;
    if (!known)
      tile = items.empty() ? 32768 : std::min(32768, items.back().first);
    tag(meta::presets::enum_choice(c,
                                   "max_tile_resolution",
                                   "Max Tile Shape",
                                   items,
                                   tile),
        "Large Graphs",
        "Largest tile evaluated at once in low memory mode");
  }

  // --- render: each section in the properties design, then its note
  meta::qt::RowContext ctx;
  ctx.theme = &bake_theme();
  ctx.default_value = [](const std::string &key) { return default_for(key); };

  meta::qt::ContainerRenderOptions render_options;
  render_options.design = properties_panel_design().design;
  render_options.row_context = ctx;
  render_options.category_policy = meta::qt::CategoryPolicy::CP_MERGED;
  render_options.root_category_name = std::string{};

  const QString note_css = QString("color: %1; font-size: 11px; background: transparent;"
                                   "padding: 0px 16px 2px 16px;")
                               .arg(HSD_CTX.app_settings.colors.text_secondary.name());

  const auto add_section = [&](meta::AttributeContainer &c, const QString &note)
  {
    QWidget *rows = meta::qt::render(c, render_options, this);
    rows->setAttribute(Qt::WA_TranslucentBackground);
    this->body()->addWidget(rows);
    this->rows.push_back(rows);

    if (!note.isEmpty())
    {
      auto *label = new QLabel(note, this);
      label->setWordWrap(true);
      label->setStyleSheet(note_css);
      this->body()->addWidget(label);
    }
  };

  this->body()->setSpacing(4);

  // --- where the files go: first, since it is the question a bake answers
  {
    const meta::qt::Theme &t = bake_theme();
    auto                  *card = new QFrame(this);
    card->setObjectName("bakeSaveTo");
    card->setStyleSheet(QString(R"(
      QFrame#bakeSaveTo { background: %1; border-radius: 10px; }
      QFrame#bakeSaveTo QLabel { color: %2; background: transparent; font-size: 13px; }
      QFrame#bakeSaveTo QLineEdit {
        background: %3; color: %2; border: 1px solid %4; border-radius: 6px;
        padding: 5px 8px; font-size: 12px; }
      QFrame#bakeSaveTo QLineEdit:focus { border-color: %5; }
      QFrame#bakeSaveTo QPushButton {
        background: transparent; color: %2; border: 1px solid %4; border-radius: 6px;
        padding: 5px 12px; font-size: 12px; }
      QFrame#bakeSaveTo QPushButton:hover { border-color: %5; }
    )")
                            .arg(t.section_surface.name(),
                                 t.ink_primary.name(),
                                 t.field.name(),
                                 t.field_border.name(),
                                 t.accent.name()));

    auto *row = new QHBoxLayout(card);
    row->setContentsMargins(20, 12, 14, 12);
    row->setSpacing(10);

    auto *label = new QLabel("Save to", card);
    label->setFixedWidth(90);
    row->addWidget(label);

    this->folder_edit = new QLineEdit(QString::fromStdString(initial_value.export_dir),
                                      card);
    this->folder_edit->setPlaceholderText(default_dir);
    this->folder_edit->setToolTip(
        QString("Folder the baked files are written to (a subfolder per extra "
                "variant).\nEmpty: %1")
            .arg(default_dir));
    this->folder_edit->setClearButtonEnabled(true);
    row->addWidget(this->folder_edit, 1);

    auto *browse = new QPushButton("Browse…", card);
    browse->setCursor(Qt::PointingHandCursor);
    row->addWidget(browse);

    QObject::connect(browse,
                     &QPushButton::clicked,
                     this,
                     [this, default_dir]()
                     {
                       const QString start = this->folder_edit->text().isEmpty()
                                                 ? default_dir
                                                 : this->folder_edit->text();
                       const QString dir = QFileDialog::getExistingDirectory(
                           this,
                           "Save baked files to",
                           start);
                       if (!dir.isEmpty())
                         this->folder_edit->setText(QDir::toNativeSeparators(dir));
                     });

    auto *wrap = new QHBoxLayout();
    wrap->setContentsMargins(14, 4, 14, 8); // aligned with the section cards
    wrap->addWidget(card);
    this->body()->addLayout(wrap);
  }

  add_section(*this->output,
              "Variants re-run the graph with randomized seeds: the same procedural "
              "setup, different results.");
  add_section(*this->options, QString());
  add_section(*this->memory,
              "Streams tiles to disk and evaluates them one after the other. Uses far "
              "less RAM on very large graphs, but bakes more slowly.");

  this->add_button("Cancel", Role::Secondary, false, true);
  this->add_button("Bake", Role::Primary, true);
}

BakeConfigDialog::~BakeConfigDialog()
{
  // the rows subscribe to the containers: they go first
  for (auto &w : this->rows)
    delete w.data();
}

BakeConfig BakeConfigDialog::get_bake_settings() const
{
  BakeConfig config;
  config.resolution = this->output->value<int>("resolution");
  config.nvariants = this->output->value<int>("nvariants");
  config.force_distributed = this->options->value<bool>("force_distributed");
  config.force_auto_export = this->options->value<bool>("force_auto_export");
  config.rename_export_files = this->options->value<bool>("rename_export_files");
  config.force_maximum_fbm_octaves = this->options->value<bool>(
      "force_maximum_fbm_octaves");
  config.min_memory = this->memory->value<bool>("min_memory");
  config.max_tile_resolution = this->memory->value<int>("max_tile_resolution");
  config.export_dir = this->folder_edit->text().trimmed().toStdString();
  return config;
}

} // namespace hesiod
