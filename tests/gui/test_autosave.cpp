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

#include "hesiod/app/autosave_manager.hpp"
#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/model/project_model.hpp"
#include "hesiod/model/utils.hpp"

using namespace hesiod;
namespace fs = std::filesystem;

namespace
{

QString qs(const std::string &s) { return QString::fromStdString(s); }

nlohmann::json sample_project_json()
{
  return nlohmann::json{{"comment", "sample"},
                        {"bake_config", nlohmann::json::object()},
                        {"graph_manager", nlohmann::json::object()}};
}

fs::path tmp_path(const AutosaveManager &m)
{
  return fs::path(m.get_snapshot_path().string() + ".tmp");
}

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

  void snapshot_key_is_stable_per_path_and_distinct_per_folder()
  {
    const fs::path a = fs::path("/tmp/x/one.hsd");
    const fs::path b = fs::path("/tmp/y/one.hsd");

    QCOMPARE(qs(AutosaveManager::snapshot_key(a)), qs(AutosaveManager::snapshot_key(a)));
    QVERIFY(AutosaveManager::snapshot_key(a) != AutosaveManager::snapshot_key(b));
    QVERIFY(AutosaveManager::snapshot_key(a).starts_with("one-"));
    QCOMPARE(AutosaveManager::snapshot_key(a).size(), size_t(4 + 8)); // "one-" + 8 hex

    const std::string untitled = "untitled-" +
                                 std::to_string(QCoreApplication::applicationPid());
    QCOMPARE(qs(AutosaveManager::snapshot_key(fs::path())), qs(untitled));
  }

  void default_directory_is_an_absolute_autosave_folder()
  {
    const fs::path dir = AutosaveManager::default_directory();
    QVERIFY(dir.is_absolute());
    QCOMPARE(qs(dir.filename().string()), QString("autosave"));
  }

  void write_snapshot_writes_project_json_plus_autosave_block_atomically()
  {
    QTemporaryDir   tmp;
    const fs::path  dir = fs::path(tmp.path().toStdString());
    AutosaveManager m(dir);
    m.set_project_json_provider(sample_project_json);
    m.set_project_path(dir / "proj.hsd");
    QSignalSpy spy(&m, &AutosaveManager::snapshot_written);

    QVERIFY(m.write_snapshot());

    QCOMPARE(spy.count(), 1);
    const fs::path snapshot = m.get_snapshot_path();
    QVERIFY(fs::exists(snapshot));
    QVERIFY(!fs::exists(tmp_path(m)));
    QCOMPARE(qs(snapshot.parent_path().string()), qs(dir.string()));
    QVERIFY(snapshot.filename().string().starts_with("proj-"));
    QVERIFY(snapshot.filename().string().ends_with(".autosave.hsd"));

    const nlohmann::json json = json_from_file(snapshot.string());
    QCOMPARE(qs(json.at("comment").get<std::string>()), QString("sample"));
    QVERIFY(json.contains("autosave"));
    const std::string
        expected_path = fs::absolute(dir / "proj.hsd").lexically_normal().string();
    QCOMPARE(qs(json["autosave"]["project_path"].get<std::string>()), qs(expected_path));
    QVERIFY(!json["autosave"]["saved_at"].get<std::string>().empty());
  }

  void snapshot_now_only_writes_pending_changes_and_keeps_flag_on_failure()
  {
    QTemporaryDir   tmp;
    const fs::path  dir = fs::path(tmp.path().toStdString());
    AutosaveManager m(dir);
    int             calls = 0;
    m.set_project_json_provider(
        [&calls]()
        {
          ++calls;
          return sample_project_json();
        });

    QVERIFY(!m.has_pending_changes());
    QVERIFY(!m.snapshot_now());
    QCOMPARE(calls, 0);

    m.mark_changed();
    QVERIFY(m.has_pending_changes());
    QVERIFY(m.snapshot_now());
    QCOMPARE(calls, 1);
    QVERIFY(!m.has_pending_changes());
    QVERIFY(fs::exists(m.get_snapshot_path()));

    QVERIFY(!m.snapshot_now());
    QCOMPARE(calls, 1);

    // a failing provider keeps the change pending and leaves no partial file
    fs::remove(m.get_snapshot_path());
    m.set_project_json_provider([]() -> nlohmann::json
                                { throw std::runtime_error("boom"); });
    m.mark_changed();
    QVERIFY(!m.snapshot_now());
    QVERIFY(m.has_pending_changes());
    QVERIFY(!fs::exists(m.get_snapshot_path()));
    QVERIFY(!fs::exists(tmp_path(m)));
  }

  void write_snapshot_without_provider_fails_cleanly()
  {
    QTemporaryDir   tmp;
    AutosaveManager m(fs::path(tmp.path().toStdString()));
    QVERIFY(!m.write_snapshot());
    QVERIFY(!fs::exists(m.get_snapshot_path()));
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
