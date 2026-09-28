/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include <QObject>
#include <QTimer>

#include "nlohmann/json.hpp"

namespace hesiod
{

// =====================================
// AutosaveManager
// =====================================

/// Periodic crash-recovery snapshots of the live project.
///
/// Owns the timer and the snapshot file; the application supplies the JSON to
/// write through a provider callback. Snapshots are ordinary .hsd files plus an
/// "autosave" block (original project path, timestamp) and live in one
/// application-owned directory, one file per project. Never throws: every
/// filesystem failure is logged and retried on the next tick.
class AutosaveManager : public QObject
{
  Q_OBJECT

public:
  struct Entry
  {
    std::filesystem::path           snapshot;     // recovery file
    std::filesystem::path           project_path; // original .hsd, empty if untitled
    std::string                     saved_at;     // from the autosave block
    std::filesystem::file_time_type modified{};   // sort tie-breaker
    bool                            readable = true;
  };

  explicit AutosaveManager(std::filesystem::path directory, QObject *parent = nullptr);

  /// <AppLocalDataLocation>/autosave, or autosave/ beside the executable in
  /// portable mode.
  static std::filesystem::path default_directory();

  /// "<stem>-<8 hex of the absolute path hash>" for a named project,
  /// "untitled-<pid>-<launch token>" for an unnamed one. The launch token is
  /// computed once per process, so a relaunch that is handed the crashed
  /// process's pid does not take the stale snapshot for its own.
  static std::string snapshot_key(const std::filesystem::path &project_path);

  /// The snapshot renamed to "<key>.deferred.autosave.hsd": where a snapshot
  /// the user chose to keep for later is parked, out of reach of this
  /// session's live snapshot. Pure; already deferred or foreign names come
  /// back unchanged.
  static std::filesystem::path deferred_path(const std::filesystem::path &snapshot);

  // --- Configuration
  void set_enabled(bool enabled);
  void set_interval(std::chrono::milliseconds interval); // <= 0 disables
  void set_project_json_provider(std::function<nlohmann::json()> provider);

  // --- Identity
  void                  set_project_path(const std::filesystem::path &path);
  std::filesystem::path get_snapshot_path() const;
  std::filesystem::path get_directory() const;

  // --- Change tracking and control
  void mark_changed();
  bool has_pending_changes() const;
  void suspend();
  void resume();
  bool is_suspended() const;

  // --- Actions
  bool snapshot_now();   // write if changed and not suspended; true when written
  bool write_snapshot(); // unconditional write; true on success
  void discard();        // remove the current snapshot file, idempotent
  void adopt(const std::filesystem::path &snapshot); // rename under the current key

  // --- Recovery
  std::vector<Entry> scan() const; // newest first, this process's untitled excluded

Q_SIGNALS:
  void snapshot_written(const QString &path);

private:
  void restart_timer();

  std::filesystem::path           directory;
  std::filesystem::path           project_path; // absolute, normalised, or empty
  std::filesystem::path           snapshot_path;
  std::function<nlohmann::json()> provider;
  QTimer                          timer;
  std::chrono::milliseconds       interval{0};
  bool                            enabled = true;
  bool                            pending = false;
  int                             suspend_depth = 0;
};

/// Suspends autosave for the current scope (batch export pumps the event loop
/// while it touches the model, so a tick must not snapshot mid-export).
class AutosaveSuspender
{
public:
  explicit AutosaveSuspender(AutosaveManager *manager) : manager(manager)
  {
    if (this->manager)
      this->manager->suspend();
  }

  ~AutosaveSuspender()
  {
    if (this->manager)
      this->manager->resume();
  }

  AutosaveSuspender(const AutosaveSuspender &) = delete;
  AutosaveSuspender &operator=(const AutosaveSuspender &) = delete;

private:
  AutosaveManager *manager;
};

} // namespace hesiod
