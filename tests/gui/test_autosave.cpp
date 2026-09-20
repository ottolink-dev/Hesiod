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

#include "hesiod/app/app_settings.hpp"
#include "hesiod/app/autosave_manager.hpp"
#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/model/graph/graph_manager.hpp"
#include "hesiod/model/graph/graph_node.hpp"
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

void patch_saved_at(const fs::path &snapshot, const std::string &saved_at)
{
  nlohmann::json json = json_from_file(snapshot.string());
  json["autosave"]["saved_at"] = saved_at;
  std::ofstream out(snapshot, std::ios::trunc);
  out << json.dump();
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

  void suspend_blocks_writes_until_fully_resumed()
  {
    QTemporaryDir   tmp;
    AutosaveManager m(fs::path(tmp.path().toStdString()));
    m.set_project_json_provider(sample_project_json);
    m.mark_changed();

    m.suspend();
    m.suspend();
    QVERIFY(m.is_suspended());
    QVERIFY(!m.snapshot_now());
    QVERIFY(m.has_pending_changes());

    m.resume();
    QVERIFY(m.is_suspended());
    QVERIFY(!m.snapshot_now());

    m.resume();
    QVERIFY(!m.is_suspended());
    QVERIFY(m.snapshot_now());

    // an extra resume never goes negative
    m.resume();
    QVERIFY(!m.is_suspended());

    {
      AutosaveSuspender guard(&m);
      QVERIFY(m.is_suspended());
    }
    QVERIFY(!m.is_suspended());

    AutosaveSuspender null_guard(nullptr); // tolerated: no manager in headless modes
  }

  void discard_removes_snapshot_and_is_idempotent()
  {
    QTemporaryDir   tmp;
    AutosaveManager m(fs::path(tmp.path().toStdString()));
    m.set_project_json_provider(sample_project_json);
    QVERIFY(m.write_snapshot());
    QVERIFY(fs::exists(m.get_snapshot_path()));

    m.discard();
    QVERIFY(!fs::exists(m.get_snapshot_path()));

    m.discard();
    QVERIFY(!fs::exists(m.get_snapshot_path()));
  }

  void set_project_path_moves_live_snapshot_to_new_key()
  {
    QTemporaryDir   tmp;
    const fs::path  dir = fs::path(tmp.path().toStdString());
    AutosaveManager m(dir);
    m.set_project_json_provider(sample_project_json);
    m.mark_changed();
    QVERIFY(m.snapshot_now());

    const fs::path old_snapshot = m.get_snapshot_path();
    QVERIFY(old_snapshot.filename().string().starts_with("untitled-"));

    m.set_project_path(dir / "named.hsd");

    QVERIFY(!fs::exists(old_snapshot));
    QVERIFY(fs::exists(m.get_snapshot_path()));
    QVERIFY(m.get_snapshot_path().filename().string().starts_with("named-"));

    // the same path again is a no-op
    const fs::path before = m.get_snapshot_path();
    m.set_project_path(dir / "named.hsd");
    QCOMPARE(qs(m.get_snapshot_path().string()), qs(before.string()));
    QVERIFY(fs::exists(before));

    // the moved file carries the new project path on its next write
    QVERIFY(m.write_snapshot());
    const nlohmann::json json = json_from_file(m.get_snapshot_path().string());
    QCOMPARE(qs(json["autosave"]["project_path"].get<std::string>()),
             qs(fs::absolute(dir / "named.hsd").lexically_normal().string()));
  }

  void adopt_moves_a_recovered_file_under_the_current_key()
  {
    QTemporaryDir  tmp;
    const fs::path dir = fs::path(tmp.path().toStdString());
    const fs::path foreign = dir / "untitled-999.autosave.hsd";
    {
      std::ofstream out(foreign);
      out << sample_project_json().dump();
    }

    AutosaveManager m(dir);
    m.adopt(foreign);

    QVERIFY(!fs::exists(foreign));
    QVERIFY(fs::exists(m.get_snapshot_path()));

    // adopting the current file, or nothing, is a no-op
    m.adopt(m.get_snapshot_path());
    QVERIFY(fs::exists(m.get_snapshot_path()));
    m.adopt(fs::path());
    QVERIFY(fs::exists(m.get_snapshot_path()));
  }

  void scan_lists_newest_first_and_flags_unreadable()
  {
    QTemporaryDir  tmp;
    const fs::path dir = fs::path(tmp.path().toStdString());

    AutosaveManager older(dir);
    older.set_project_json_provider(sample_project_json);
    older.set_project_path(dir / "a" / "one.hsd");
    QVERIFY(older.write_snapshot());
    patch_saved_at(older.get_snapshot_path(), "2026-01-01_00-00-00");

    AutosaveManager newer(dir);
    newer.set_project_json_provider(sample_project_json);
    newer.set_project_path(dir / "b" / "two.hsd");
    QVERIFY(newer.write_snapshot());
    patch_saved_at(newer.get_snapshot_path(), "2026-02-02_00-00-00");

    // noise: a corrupt snapshot, an in-progress temp file, an unrelated file
    {
      std::ofstream(dir / "garbage.autosave.hsd") << "{ not json";
      std::ofstream(dir / "partial.autosave.hsd.tmp") << "{}";
      std::ofstream(dir / "notes.txt") << "hello";
    }

    AutosaveManager scanner(dir);
    const auto      entries = scanner.scan();

    QCOMPARE(entries.size(), size_t(3));

    QVERIFY(entries[0].readable);
    QCOMPARE(qs(entries[0].saved_at), QString("2026-02-02_00-00-00"));
    QCOMPARE(qs(entries[0].project_path.string()),
             qs(fs::absolute(dir / "b" / "two.hsd").lexically_normal().string()));
    QCOMPARE(qs(entries[0].snapshot.string()), qs(newer.get_snapshot_path().string()));

    QVERIFY(entries[1].readable);
    QCOMPARE(qs(entries[1].saved_at), QString("2026-01-01_00-00-00"));
    QCOMPARE(qs(entries[1].project_path.string()),
             qs(fs::absolute(dir / "a" / "one.hsd").lexically_normal().string()));

    QVERIFY(!entries[2].readable);
    QCOMPARE(qs(entries[2].snapshot.filename().string()),
             QString("garbage.autosave.hsd"));
  }

  void scan_skips_own_untitled_snapshot_and_missing_directory()
  {
    QTemporaryDir   tmp;
    const fs::path  dir = fs::path(tmp.path().toStdString()) / "nested" / "autosave";
    AutosaveManager m(dir);
    QVERIFY(m.scan().empty()); // directory does not exist yet

    m.set_project_json_provider(sample_project_json);
    QVERIFY(m.write_snapshot());
    QVERIFY(fs::exists(m.get_snapshot_path()));
    QVERIFY(m.scan().empty()); // this process's own untitled file

    m.set_project_path(dir / "named.hsd");
    QCOMPARE(m.scan().size(), size_t(1)); // a named snapshot is always listed
  }

  void timer_writes_when_enabled_and_changed()
  {
    QTemporaryDir   tmp;
    AutosaveManager m(fs::path(tmp.path().toStdString()));
    m.set_project_json_provider(sample_project_json);
    QSignalSpy spy(&m, &AutosaveManager::snapshot_written);

    m.set_interval(std::chrono::milliseconds(20));
    m.mark_changed();

    QVERIFY(spy.wait(2000));
    QCOMPARE(spy.count(), 1);
    QVERIFY(fs::exists(m.get_snapshot_path()));

    // nothing changed: the timer keeps ticking but writes nothing
    QVERIFY(!spy.wait(150));
    QCOMPARE(spy.count(), 1);
  }

  void timer_is_idle_when_disabled_or_interval_is_zero()
  {
    QTemporaryDir   tmp;
    AutosaveManager m(fs::path(tmp.path().toStdString()));
    m.set_project_json_provider(sample_project_json);
    QSignalSpy spy(&m, &AutosaveManager::snapshot_written);

    m.set_interval(std::chrono::milliseconds(0));
    m.mark_changed();
    QVERIFY(!spy.wait(100));
    QVERIFY(!fs::exists(m.get_snapshot_path()));

    m.set_enabled(false);
    m.set_interval(std::chrono::milliseconds(20));
    QVERIFY(!spy.wait(100));
    QVERIFY(!fs::exists(m.get_snapshot_path()));

    m.set_enabled(true);
    QVERIFY(spy.wait(2000));
    QVERIFY(fs::exists(m.get_snapshot_path()));
  }

  void settings_round_trip_autosave_keys()
  {
    AppSettings defaults;
    QVERIFY(defaults.global.enable_autosave);
    QCOMPARE(defaults.global.autosave_interval_s, 120);

    AppSettings s;
    s.global.enable_autosave = false;
    s.global.autosave_interval_s = 45;

    const nlohmann::json json = s.json_to();
    QCOMPARE(json.at("global.enable_autosave").get<bool>(), false);
    QCOMPARE(json.at("global.autosave_interval_s").get<int>(), 45);

    AppSettings t;
    t.json_from(json);
    QVERIFY(!t.global.enable_autosave);
    QCOMPARE(t.global.autosave_interval_s, 45);
  }

  void snapshot_round_trips_through_project_model()
  {
    auto config = std::make_shared<GraphConfig>();
    config->set_shape({8, 8});
    config->set_tiling({1, 1});
    config->set_overlap(0.f);
    config->storage_mode = hmap::StorageMode::VA_RAM;

    ProjectModel model;
    auto         graph = std::make_shared<GraphNode>("graph", config);
    model.get_graph_manager_ref()->add_graph_node(graph, "graph");
    const std::string a = graph->add_node("Thru");
    const std::string b = graph->add_node("Thru");
    QVERIFY(graph->new_link(a, "output", b, "input"));

    QTemporaryDir   tmp;
    AutosaveManager m(fs::path(tmp.path().toStdString()));
    m.set_project_json_provider([&model]() { return model.json_to(); });
    QVERIFY(m.write_snapshot());

    ProjectModel restored;
    restored.json_from(json_from_file(m.get_snapshot_path().string()));

    GraphNode *rg = restored.get_graph_manager_ref()->get_graph_ref_by_id("graph");
    QVERIFY(rg);
    QCOMPARE(rg->get_nodes().size(), size_t(2));
    QCOMPARE(rg->get_links().size(), size_t(1));
    QVERIFY(rg->get_node(a));
    QVERIFY(rg->get_node(b));
    QCOMPARE(qs(rg->get_links().front().from), qs(a));
    QCOMPARE(qs(rg->get_links().front().to), qs(b));
  }

  void autosave_manager_is_absent_in_context_only_mode()
  {
    QVERIFY(HSD_APP->get_autosave_manager_ref() == nullptr);
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
