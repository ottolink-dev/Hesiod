/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <filesystem>
#include <fstream>
#include <stdexcept>

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "nlohmann/json.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/model/project_model.hpp"
#include "hesiod/model/utils.hpp"

using namespace hesiod;
namespace fs = std::filesystem;

namespace
{

QString qs(const std::string &s) { return QString::fromStdString(s); }

} // namespace

class AutosaveTest : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void project_model_has_changed_fires_on_every_change()
  {
    ProjectModel model;
    int          calls = 0;
    model.has_changed = [&calls]() { ++calls; };

    model.on_has_changed();
    model.on_has_changed();

    QCOMPARE(calls, 2);
    QVERIFY(model.get_is_dirty());
  }

  void project_file_json_has_model_and_header_without_ui()
  {
    AppContext &ctx = HSD_CTX;
    ctx.new_project();
    ctx.project_model->set_comment("hello");

    const nlohmann::json json = HSD_APP->project_file_json();

    QCOMPARE(qs(json.at("comment").get<std::string>()), QString("hello"));
    QVERIFY(json.contains("graph_manager"));
    QVERIFY(json.contains("bake_config"));
    QVERIFY(json.contains("Hesiod version"));
    QVERIFY(json.contains("saved_at"));
    // ContextOnly mode has no ProjectUI, so no UI state is emitted
    QVERIFY(!json.contains("graph_manager_widget"));
    QVERIFY(!json.contains("graph_tabs_widget"));
  }
};

int main(int argc, char **argv)
{
  // Real application context and node factory, no main window. Isolate the
  // per-user configuration and data directories for the test process.
  QTemporaryDir config_dir;
  qputenv("XDG_CONFIG_HOME", config_dir.path().toUtf8());
  qputenv("XDG_CACHE_HOME", config_dir.path().toUtf8());
  qputenv("XDG_DATA_HOME", config_dir.path().toUtf8());
  qputenv("QT_LOGGING_RULES", HESIOD_QPUTENV_QT_LOGGING_RULES);
  HesiodApplication app(argc, argv, HesiodApplication::StartupMode::ContextOnly);
  AutosaveTest      tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "test_autosave.moc"
