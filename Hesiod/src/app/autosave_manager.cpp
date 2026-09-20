/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <cstdint>
#include <format>
#include <fstream>
#include <stdexcept>
#include <system_error>

#include <QCoreApplication>
#include <QStandardPaths>

#include "hesiod/app/app_context.hpp"
#include "hesiod/app/autosave_manager.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/utils.hpp"

namespace fs = std::filesystem;

namespace hesiod
{

namespace
{

constexpr const char *snapshot_suffix = ".autosave.hsd";
constexpr const char *tmp_suffix = ".tmp";

// fs::absolute(p) throws if current_path() fails (e.g. a deleted/inaccessible
// cwd); fall back to the input path, normalised, rather than propagate.
fs::path normalised_absolute(const fs::path &p)
{
  std::error_code ec;
  fs::path        abs = fs::absolute(p, ec);
  return (ec ? p : abs).lexically_normal();
}

} // namespace

AutosaveManager::AutosaveManager(fs::path directory, QObject *parent)
    : QObject(parent), directory(std::move(directory))
{
  Logger::log()->trace("AutosaveManager::AutosaveManager: {}", this->directory.string());

  this->set_project_path(fs::path());

  this->timer.setTimerType(Qt::VeryCoarseTimer);
  this->connect(&this->timer, &QTimer::timeout, this, [this]() { this->snapshot_now(); });
}

fs::path AutosaveManager::default_directory()
{
  if (is_portable_mode())
    return fs::path(QCoreApplication::applicationDirPath().toStdString()) / "autosave";

  QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
  if (dir.isEmpty())
    dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

  return fs::path(dir.toStdString()) / "autosave";
}

std::string AutosaveManager::snapshot_key(const fs::path &project_path)
{
  if (project_path.empty())
    return std::format("untitled-{}", QCoreApplication::applicationPid());

  const fs::path abs = normalised_absolute(project_path);
  const size_t   hash = std::hash<std::string>{}(abs.generic_string());
  return std::format("{}-{:08x}", abs.stem().string(), static_cast<uint32_t>(hash));
}

// --- Configuration

void AutosaveManager::set_enabled(bool enabled)
{
  this->enabled = enabled;
  this->restart_timer();
}

void AutosaveManager::set_interval(std::chrono::milliseconds interval)
{
  this->interval = interval;
  this->restart_timer();
}

void AutosaveManager::set_project_json_provider(std::function<nlohmann::json()> provider)
{
  this->provider = std::move(provider);
}

void AutosaveManager::restart_timer()
{
  this->timer.stop();

  if (this->enabled && this->interval.count() > 0)
    this->timer.start(this->interval);
}

// --- Identity

void AutosaveManager::set_project_path(const fs::path &path)
{
  const fs::path new_project_path = path.empty() ? fs::path() : normalised_absolute(path);
  const fs::path new_snapshot = this->directory /
                                (snapshot_key(new_project_path) + snapshot_suffix);

  this->project_path = new_project_path;

  if (new_snapshot == this->snapshot_path)
    return;

  // keep the live snapshot when the project is renamed (untitled -> save as)
  std::error_code ec;
  if (!this->snapshot_path.empty() && fs::exists(this->snapshot_path, ec))
  {
    fs::rename(this->snapshot_path, new_snapshot, ec);
    if (ec)
      Logger::log()->warn(
          "AutosaveManager::set_project_path: could not move {} to {}: {}",
          this->snapshot_path.string(),
          new_snapshot.string(),
          ec.message());
  }

  this->snapshot_path = new_snapshot;
}

fs::path AutosaveManager::get_snapshot_path() const { return this->snapshot_path; }

fs::path AutosaveManager::get_directory() const { return this->directory; }

// --- Change tracking and control

void AutosaveManager::mark_changed() { this->pending = true; }

bool AutosaveManager::has_pending_changes() const { return this->pending; }

void AutosaveManager::suspend() { ++this->suspend_depth; }

void AutosaveManager::resume()
{
  if (this->suspend_depth > 0)
    --this->suspend_depth;
}

bool AutosaveManager::is_suspended() const { return this->suspend_depth > 0; }

// --- Actions

bool AutosaveManager::snapshot_now()
{
  if (!this->pending || this->is_suspended())
    return false;

  if (!this->write_snapshot())
    return false;

  this->pending = false;
  return true;
}

bool AutosaveManager::write_snapshot()
{
  if (!this->provider)
  {
    Logger::log()->warn("AutosaveManager::write_snapshot: no project JSON provider");
    return false;
  }

  std::string payload;

  try
  {
    nlohmann::json json = this->provider();
    json["autosave"] = {{"project_path", this->project_path.string()},
                        {"saved_at", timestamp()}};
    payload = json.dump(4, ' ', false, nlohmann::json::error_handler_t::replace);
  }
  catch (const std::exception &e)
  {
    Logger::log()->warn("AutosaveManager::write_snapshot: could not serialize the "
                        "project: {}",
                        e.what());
    return false;
  }

  std::error_code ec;
  fs::create_directories(this->directory, ec);
  if (ec)
  {
    Logger::log()->warn("AutosaveManager::write_snapshot: could not create {}: {}",
                        this->directory.string(),
                        ec.message());
    return false;
  }

  // write to a sibling temp file, then rename over the final name so a crash
  // mid-write can never leave a truncated recovery file behind
  const fs::path tmp = fs::path(this->snapshot_path.string() + tmp_suffix);

  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out.is_open())
    {
      Logger::log()->warn("AutosaveManager::write_snapshot: could not open {} for "
                          "writing",
                          tmp.string());
      return false;
    }

    out << payload;
    out.flush();

    if (!out.good())
    {
      Logger::log()->warn("AutosaveManager::write_snapshot: write to {} failed",
                          tmp.string());
      out.close();
      fs::remove(tmp, ec);
      return false;
    }
  }

  fs::rename(tmp, this->snapshot_path, ec);
  if (ec)
  {
    Logger::log()->warn("AutosaveManager::write_snapshot: could not move {} to {}: {}",
                        tmp.string(),
                        this->snapshot_path.string(),
                        ec.message());
    fs::remove(tmp, ec);
    return false;
  }

  Logger::log()->trace("AutosaveManager::write_snapshot: {}",
                       this->snapshot_path.string());

  Q_EMIT this->snapshot_written(QString::fromStdString(this->snapshot_path.string()));
  return true;
}

void AutosaveManager::discard()
{
  std::error_code ec;

  if (fs::exists(this->snapshot_path, ec))
  {
    fs::remove(this->snapshot_path, ec);
    if (ec)
      Logger::log()->warn("AutosaveManager::discard: could not remove {}: {}",
                          this->snapshot_path.string(),
                          ec.message());
    else
      Logger::log()->trace("AutosaveManager::discard: {}", this->snapshot_path.string());
  }

  // a stray temp file from an interrupted write is never worth keeping
  fs::remove(fs::path(this->snapshot_path.string() + tmp_suffix), ec);

  // whatever was pending is saved or gone
  this->pending = false;
}

void AutosaveManager::adopt(const fs::path &snapshot)
{
  if (snapshot.empty())
    return;

  const fs::path from = normalised_absolute(snapshot);
  const fs::path to = normalised_absolute(this->snapshot_path);

  if (from == to)
    return;

  std::error_code ec;
  fs::create_directories(this->directory, ec);
  fs::rename(from, to, ec);

  if (ec)
    Logger::log()->warn("AutosaveManager::adopt: could not move {} to {}: {}",
                        from.string(),
                        to.string(),
                        ec.message());
  else
    Logger::log()->trace("AutosaveManager::adopt: {} -> {}", from.string(), to.string());
}

// --- Recovery

std::vector<AutosaveManager::Entry> AutosaveManager::scan() const
{
  std::vector<Entry> entries;
  std::error_code    ec;

  if (!fs::is_directory(this->directory, ec))
    return entries;

  const std::string own_untitled = snapshot_key(fs::path()) + snapshot_suffix;

  try
  {
    for (const fs::directory_entry &it : fs::directory_iterator(this->directory, ec))
    {
      const fs::path    path = it.path();
      const std::string name = path.filename().string();

      // ends_with also rejects in-progress "*.autosave.hsd.tmp" files
      if (!name.ends_with(snapshot_suffix) || name == own_untitled)
        continue;

      Entry entry;
      entry.snapshot = path;
      entry.modified = fs::last_write_time(path, ec);

      try
      {
        const nlohmann::json json = json_from_file(path.string());

        if (!json.is_object() || !json.contains("graph_manager"))
          throw std::runtime_error("not a project file");

        if (json.contains("autosave"))
        {
          entry.project_path = fs::path(json["autosave"].value("project_path", ""));
          entry.saved_at = json["autosave"].value("saved_at", "");
        }
      }
      catch (const std::exception &e)
      {
        Logger::log()->warn("AutosaveManager::scan: unreadable recovery file {}: {}",
                            path.string(),
                            e.what());
        entry.readable = false;
      }

      entries.push_back(std::move(entry));
    }
  }
  catch (const std::exception &e)
  {
    Logger::log()->warn("AutosaveManager::scan: could not list {}: {}",
                        this->directory.string(),
                        e.what());
  }

  // timestamps are "%Y-%m-%d_%H-%M-%S", so string order is time order;
  // unreadable files have none and sink to the end
  std::sort(entries.begin(),
            entries.end(),
            [](const Entry &a, const Entry &b)
            {
              if (a.saved_at != b.saved_at)
                return a.saved_at > b.saved_at;
              return a.modified > b.modified;
            });

  return entries;
}

} // namespace hesiod
