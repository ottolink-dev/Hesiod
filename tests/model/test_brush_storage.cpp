/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <cmath>
#include <memory>

#include <QTemporaryDir>
#include <QtTest>

#include "meta/ext/array/array.hpp"
#include "meta/metadata/keys.hpp"
#include "meta_qt/widget_renderer.hpp"
#include "meta_qt/widgets/array_canvas.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/model/nodes/node_factory.hpp"

using namespace hesiod;

namespace
{
std::shared_ptr<GraphConfig> config_at(glm::ivec2 shape)
{
  auto config = std::make_shared<GraphConfig>();
  config->set_shape(shape);
  config->set_tiling({1, 1});
  config->set_overlap(0.f);
  config->storage_mode = hmap::StorageMode::VA_RAM;
  return config;
}

std::shared_ptr<BaseNode> brush(std::shared_ptr<GraphConfig> config)
{
  return std::dynamic_pointer_cast<BaseNode>(node_factory("Brush", config));
}

meta::Attribute<meta::Array> &painting(BaseNode &node)
{
  return *node.get_meta_group()
              .current()
              .find("hmap")
              ->try_cast<meta::Attribute<meta::Array>>();
}
} // namespace

class BrushStorageTest : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void init() { HSD_CTX.app_settings.node_editor.brush_storage_resolution = 512; }

  void storage_and_saved_size_do_not_follow_graph_resolution()
  {
    auto        small_config = config_at({256, 128});
    auto        large_config = config_at({4096, 4096});
    auto        small = brush(small_config);
    auto        large = brush(large_config);
    const auto &a = painting(*large).value();
    QCOMPARE(a.shape.x, 512);
    QCOMPARE(a.shape.y, 512);
    QCOMPARE(a.vector.size() * sizeof(float), size_t(1024 * 1024));
    QCOMPARE(painting(*large).metadata().value<int>(meta::keys::ui::width), 256);
    QCOMPARE(painting(*large).metadata().value<int>(meta::keys::ui::height), 256);
    const auto saved = a.json_to().dump();
    QCOMPARE(saved, painting(*small).value().json_to().dump());
    QVERIFY(saved.size() < 4 * 1024 * 1024);

    large_config->set_shape({8192, 4096});
    QCOMPARE(a.json_to().dump(), saved);
  }

  void settings_control_only_new_brushes()
  {
    auto config = config_at({4096, 4096});
    auto original = brush(config);
    HSD_CTX.app_settings.json_from({{"node_editor.brush_storage_resolution", 1024}});
    auto larger = brush(config);
    QCOMPARE(painting(*larger).value().shape.x, 1024);
    QCOMPARE(painting(*original).value().shape.x, 512);
    QCOMPARE(painting(*larger).metadata().value<int>(meta::keys::ui::width), 256);

    AppSettings restored;
    restored.json_from(HSD_CTX.app_settings.json_to());
    QCOMPARE(restored.node_editor.brush_storage_resolution, 1024);
    for (int invalid : {-1, 0, 127, 4097, 2147483647})
    {
      restored.json_from({{"node_editor.brush_storage_resolution", invalid}});
      QCOMPARE(restored.node_editor.brush_storage_resolution, 512);
    }
  }

  void saved_paintings_keep_shape_and_samples()
  {
    auto config = config_at({32, 32});
    auto source = brush(config);
    // Rectangular data at a resolution above the new default exercises both
    // compatibility and the absence of implicit downsampling on load.
    auto &a = painting(*source).value();
    a.shape = {1024, 768};
    a.vector.resize(1024 * 768);
    for (size_t i = 0; i < a.vector.size(); ++i)
      a.vector[i] = static_cast<float>(i % 997) / 996.f;

    auto loaded = brush(config);
    loaded->json_from(nlohmann::json::parse(source->json_to().dump()));
    QCOMPARE(painting(*loaded).value().shape.x, a.shape.x);
    QCOMPARE(painting(*loaded).value().shape.y, a.shape.y);
    QVERIFY(painting(*loaded).value().vector == a.vector);

    // Pre-Meta projects stored the painting at the node's top level.
    auto legacy = brush(config);
    legacy->json_from(
        {{"label", "Brush"},
         {"hmap",
          {{"shape.x", a.shape.x}, {"shape.y", a.shape.y}, {"vector", a.vector}}}});
    QCOMPARE(painting(*legacy).value().shape.x, a.shape.x);
    QCOMPARE(painting(*legacy).value().shape.y, a.shape.y);
    QVERIFY(painting(*legacy).value().vector == a.vector);
  }

  void canvas_edits_round_trip_at_storage_resolution()
  {
    auto                                  config = config_at({4096, 4096});
    auto                                  node = brush(config);
    std::unique_ptr<meta::qt::MetaWidget> widget(meta::qt::render(&painting(*node)));
    auto *canvas = widget->findChild<meta::qt::ArrayCanvas *>();
    QVERIFY(canvas);
    QCOMPARE(canvas->get_field_width(), 256);
    QCOMPARE(canvas->get_field_height(), 256);
    // Drive the editor's normal commit path with a nonzero canvas field.
    canvas->set_field_data(std::vector<float>(256 * 256, 0.375f));
    QVERIFY(QMetaObject::invokeMethod(canvas, "edit_ended", Qt::DirectConnection));
    const auto &a = painting(*node).value();
    QCOMPARE(a.shape.x, 512);
    QCOMPARE(a.vector.size(), size_t(512 * 512));
    QVERIFY(std::all_of(a.vector.begin(),
                        a.vector.end(),
                        [](float v) { return std::abs(v - 0.375f) < 1.e-5f; }));
    auto loaded = brush(config);
    loaded->json_from(nlohmann::json::parse(node->json_to().dump()));
    QVERIFY(painting(*loaded).value().vector == a.vector);
  }

  void compute_resamples_without_modifying_the_painting()
  {
    auto  config = config_at({32, 16});
    auto  node = brush(config);
    auto &a = painting(*node).value();
    node->set_state("post_remap", meta::keys::state::active, false);
    std::fill(a.vector.begin(), a.vector.end(), 0.375f);
    const auto saved = a.json_to();
    compute_brush_node(*node);
    const auto output = node->get_value_ref<hmap::VirtualArray>("output")->to_array(
        config->cm_cpu);
    QCOMPARE(output.shape.x, 32);
    QCOMPARE(output.shape.y, 16);
    QVERIFY(std::all_of(output.vector.begin(),
                        output.vector.end(),
                        [](float v) { return std::abs(v - 0.375f) < 1.e-5f; }));
    QVERIFY(a.json_to() == saved);

    // Keep the existing malformed-data fallback working after removing the
    // unconditional full-resolution temporary allocation.
    a.shape = {0, 0};
    a.vector.clear();
    compute_brush_node(*node);
    const auto fallback = node->get_value_ref<hmap::VirtualArray>("output")->to_array(
        config->cm_cpu);
    QCOMPARE(fallback.shape.x, 32);
    QVERIFY(std::all_of(fallback.vector.begin(),
                        fallback.vector.end(),
                        [](float v) { return v == 0.f; }));
  }
};

int main(int argc, char **argv)
{
  QTemporaryDir config_dir;
  qputenv("XDG_CONFIG_HOME", config_dir.path().toUtf8());
  qputenv("XDG_CACHE_HOME", config_dir.path().toUtf8());
  qputenv("QT_LOGGING_RULES", HESIOD_QPUTENV_QT_LOGGING_RULES);
  HesiodApplication app(argc, argv, HesiodApplication::StartupMode::ContextOnly);
  BrushStorageTest  tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "test_brush_storage.moc"
