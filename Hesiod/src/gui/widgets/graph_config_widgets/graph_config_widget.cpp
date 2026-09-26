/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <cmath>
#include <format>
#include <string>

#include <QLabel>
#include <QVBoxLayout>

#include "meta/core/attribute_container.hpp"
#include "meta/metadata/keys.hpp"
#include "meta/presets/choice.hpp"
#include "meta/presets/numeric.hpp"
#include "meta_qt/container_widget.hpp"
#include "meta_qt/ui/theme.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/graph_config_widgets/graph_config_dialog.hpp"
#include "hesiod/gui/widgets/properties_panel_design.hpp"
#include "hesiod/logger.hpp"

namespace hesiod
{

namespace
{

// the aspect presets: {w, h}; {0, 0} is Custom (width and height chosen apart)
const std::vector<std::pair<glm::ivec2, std::string>> kAspects = {
    {{1, 1}, "1:1  (square)"},
    {{2, 1}, "2:1  (equirectangular)"},
    {{3, 2}, "3:2"},
    {{16, 9}, "16:9"},
    {{0, 0}, "Custom"}};

constexpr int kShapeSnap = 16; // short edges stay tiling-friendly

const meta::qt::Theme &config_theme()
{
  static const meta::qt::Theme theme = []()
  {
    meta::qt::Theme t = *properties_panel_design().theme;
    t.group_accents = {{"Domain", QColor("#7d9cc0")}, {"Computation", QColor("#3aa899")}};
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

std::string side_label(int size)
{
  return size >= 1024 ? std::format("{}  ·  {}K", size, size / 1024)
                      : std::to_string(size);
}

std::vector<std::pair<int, std::string>> side_items(int keep)
{
  std::vector<std::pair<int, std::string>> items;
  for (int size = 256; size <= 8192; size *= 2)
    items.push_back({size, side_label(size)});
  // a current size outside the list stays selectable as itself
  if (std::none_of(items.begin(),
                   items.end(),
                   [keep](const auto &e) { return e.first == keep; }))
    items.push_back({keep, std::to_string(keep)});
  return items;
}

glm::ivec2 shape_for(int aspect_index, int long_edge, int custom_height)
{
  const glm::ivec2 ratio = kAspects[std::clamp(aspect_index, 0, int(kAspects.size()) - 1)]
                               .first;
  if (ratio.x == 0 || ratio.y == 0)
    return {long_edge, custom_height};

  const auto snap = [](int v)
  { return std::max(kShapeSnap, (v / kShapeSnap) * kShapeSnap); };
  if (ratio.x >= ratio.y)
    return {long_edge, snap(long_edge * ratio.y / ratio.x)};
  return {snap(long_edge * ratio.x / ratio.y), long_edge};
}

} // namespace

GraphConfigDialog::GraphConfigDialog(GraphConfig &config, QWidget *parent)
    : MessageDialog(parent,
                    Kind::None,
                    "Graph configuration",
                    "The resolution this graph is computed at, and how the work is "
                    "spread over the CPU and the GPU."),
      config(config), domain(std::make_unique<meta::AttributeContainer>()),
      compute(std::make_unique<meta::AttributeContainer>())
{
  this->setWindowTitle("Hesiod - Graph configuration");
  this->set_card_width(500);

  // --- domain: work out which preset the current shape is
  {
    const glm::ivec2 s = config.shape;
    const int        long_edge = std::max(s.x, s.y);
    int              aspect = int(kAspects.size()) - 1; // Custom
    for (int i = 0; i + 1 < int(kAspects.size()); ++i)
      if (shape_for(i, long_edge, s.y) == s)
      {
        aspect = i;
        break;
      }
    const bool custom = aspect == int(kAspects.size()) - 1;

    std::vector<std::pair<int, std::string>> aspect_items;
    for (int i = 0; i < int(kAspects.size()); ++i)
      aspect_items.push_back({i, kAspects[i].second});

    auto &c = *this->domain;
    tag(meta::presets::enum_choice(c, "aspect", "Aspect", aspect_items, aspect),
        "Domain",
        "Width to height ratio of the heightmaps");
    tag(meta::presets::enum_choice(c,
                                   "resolution",
                                   "Resolution",
                                   side_items(custom ? s.x : long_edge),
                                   custom ? s.x : long_edge),
        "Domain",
        "Size of the longest side, in pixels (the width for a custom aspect)");
    tag(meta::presets::enum_choice(c, "height", "Custom Height", side_items(s.y), s.y),
        "Domain",
        "Height in pixels, used with the Custom aspect only");
  }

  // --- computation
  {
    auto &c = *this->compute;

    std::vector<std::pair<int, std::string>> tiling_items;
    for (int t = 1; t <= 16; t *= 2)
      tiling_items.push_back({t, std::format("{0} × {0}", t)});
    tag(meta::presets::enum_choice(c, "tiling", "Tiling", tiling_items, config.tiling.x),
        "Computation",
        "Tiles the domain is split into, computed in parallel");

    const int overlap = int(std::lround(config.overlap * 100.f));
    std::vector<std::pair<int, std::string>> overlap_items;
    for (int o : {0, 25, 50, 75})
      overlap_items.push_back({o, std::format("{} %", o)});
    if (overlap % 25 != 0)
      overlap_items.push_back({overlap, std::format("{} %", overlap)});
    tag(meta::presets::enum_choice(c, "overlap", "Tile Overlap", overlap_items, overlap),
        "Computation",
        "How far neighbouring tiles overlap, which hides seams");

    tag(meta::presets::toggle_button(c,
                                     "cache_on_disk",
                                     "Cache on Disk",
                                     config.cm_cpu.trim_storage),
        "Computation",
        "Reduce memory use by caching node data on disk");

    std::vector<std::pair<int, std::string>> mode_items;
    for (const auto &[name, id] : hmap::for_each_mode_as_string)
      mode_items.push_back({id, name});
    tag(meta::presets::enum_choice(c,
                                   "cpu_mode",
                                   "CPU",
                                   mode_items,
                                   int(config.cm_cpu.mode)),
        "Computation",
        "How CPU nodes walk the tiles");
    tag(meta::presets::enum_choice(c,
                                   "gpu_mode",
                                   "GPU",
                                   mode_items,
                                   int(config.cm_gpu.mode)),
        "Computation",
        "How GPU nodes walk the tiles");
  }

  // --- render the two sections, each with its note
  meta::qt::RowContext ctx;
  ctx.theme = &config_theme();

  meta::qt::ContainerRenderOptions options;
  options.design = properties_panel_design().design;
  options.row_context = ctx;
  options.category_policy = meta::qt::CategoryPolicy::CP_MERGED;
  options.root_category_name = std::string{};

  const QString note_css = QString("color: %1; font-size: 11px; background: transparent;"
                                   "padding: 0px 16px 2px 16px;")
                               .arg(HSD_CTX.app_settings.colors.text_secondary.name());

  const auto add_section = [&](meta::AttributeContainer &c, QLabel *note)
  {
    QWidget *widget = meta::qt::render(c, options, this);
    widget->setAttribute(Qt::WA_TranslucentBackground);
    this->body()->addWidget(widget);
    this->rows.push_back(widget);
    if (note)
    {
      note->setWordWrap(true);
      note->setStyleSheet(note_css);
      this->body()->addWidget(note);
    }
  };

  this->body()->setSpacing(4);

  this->summary = new QLabel(this);
  add_section(*this->domain, this->summary);
  add_section(*this->compute,
              new QLabel("These change how the graph is computed (tiles, threads, the "
                         "GPU), never the result itself.",
                         this));

  // the size the choices add up to, live
  for (const char *key : {"aspect", "resolution", "height"})
    if (auto *a = dynamic_cast<meta::Attribute<int> *>(this->domain->find(key)))
      this->connections.push_back(
          a->value_changed.subscribe([this](const int &) { this->update_summary(); }));
  this->update_summary();

  this->add_button("Cancel", Role::Secondary, false, true);
  this->add_button("Apply", Role::Primary, true);
}

GraphConfigDialog::~GraphConfigDialog()
{
  // the rows subscribe to the containers: they go first
  this->connections.clear();
  for (auto &w : this->rows)
    delete w.data();
}

glm::ivec2 GraphConfigDialog::shape() const
{
  return shape_for(this->domain->value<int>("aspect"),
                   this->domain->value<int>("resolution"),
                   this->domain->value<int>("height"));
}

void GraphConfigDialog::update_summary()
{
  const glm::ivec2 s = this->shape();
  const bool   custom = this->domain->value<int>("aspect") == int(kAspects.size()) - 1;
  const double megapixels = double(s.x) * double(s.y) / 1.0e6;

  this->summary->setText(
      QString("Heightmaps of %1 × %2 px  ·  %3 MP%4")
          .arg(s.x)
          .arg(s.y)
          .arg(megapixels, 0, 'f', megapixels < 10.0 ? 1 : 0)
          .arg(custom ? "" : ".  Custom Height applies to the Custom aspect only."));
}

void GraphConfigDialog::accept()
{
  this->config.shape = this->shape();

  const int tiling = this->compute->value<int>("tiling");
  this->config.tiling = {tiling, tiling};
  this->config.overlap = float(this->compute->value<int>("overlap")) / 100.f;

  const bool cache = this->compute->value<bool>("cache_on_disk");
  this->config.cm_cpu.trim_storage = cache;
  this->config.cm_gpu.trim_storage = cache;
  this->config.cm_single_array.trim_storage = cache;

  this->config.cm_cpu.mode = static_cast<hmap::ForEachMode>(
      this->compute->value<int>("cpu_mode"));
  this->config.cm_gpu.mode = static_cast<hmap::ForEachMode>(
      this->compute->value<int>("gpu_mode"));

  Logger::log()->trace("GraphConfigDialog::accept: shape {}x{}, tiling {}, overlap {}",
                       this->config.shape.x,
                       this->config.shape.y,
                       tiling,
                       this->config.overlap);

  MessageDialog::accept();
}

} // namespace hesiod
