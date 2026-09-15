/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>

#include <QByteArray>
#include <QDir>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QtGlobal>

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include "nlohmann/json.hpp"

#include "hesiod/app/ui_scale.hpp"
#include "hesiod/logger.hpp"

namespace fs = std::filesystem;

namespace hesiod::ui_scale
{

namespace
{
Resolution g_session;
} // namespace

double sanitize(double value, bool *out_was_clean)
{
  const bool finite = std::isfinite(value);
  const bool in_range = finite && value >= kMin && value <= kMax;

  if (out_was_clean)
    *out_was_clean = in_range;

  if (!finite)
    return kDefault;

  return std::clamp(value, kMin, kMax);
}

std::string executable_dir(const char *argv0)
{
  std::error_code ec;

#ifdef Q_OS_WIN
  // argv[0] is whatever the launcher passed: a bare "hesiod.exe" found on PATH,
  // or a path relative to a working directory that is not the install. Ask the
  // loader for the real image path instead.
  {
    std::wstring buffer(1024, L'\0');
    const DWORD  length = ::GetModuleFileNameW(nullptr,
                                              buffer.data(),
                                              static_cast<DWORD>(buffer.size()));

    // length == size means truncated, which is not a path we should trust
    if (length > 0 && length < buffer.size())
    {
      buffer.resize(length);
      const fs::path exe(buffer);
      if (exe.has_parent_path())
        return exe.parent_path().string();
    }
  }
#endif

  if (argv0 && *argv0)
  {
    const fs::path exe = fs::absolute(fs::path(argv0), ec);
    if (!ec)
      return exe.parent_path().string();
  }

  return {};
}

std::string startup_config_path(const char *argv0)
{
  // portable install: a config or a flag file sitting next to the executable
  std::error_code ec;
  const fs::path  exe_dir = executable_dir(argv0);

  if (!exe_dir.empty())
  {
    const fs::path portable_json = exe_dir / "hesiod.json";
    const fs::path portable_flag = exe_dir / "portable.flag";

    if (fs::exists(portable_json, ec) || fs::exists(portable_flag, ec))
      return portable_json.string();
  }

  // per-user config; QStandardPaths needs the application name, which main()
  // pins before calling us so this matches what AppContext resolves later
  QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
  if (dir.isEmpty())
    dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

  if (dir.isEmpty())
    return {};

  return (dir + QDir::separator() + "hesiod.json").toStdString();
}

double scale_from_config(const std::string &config_path)
{
  if (config_path.empty())
    return kDefault;

  std::error_code ec;
  if (!fs::exists(fs::path(config_path), ec))
    return kDefault;

  std::ifstream file(config_path);
  if (!file.is_open())
    return kDefault;

  nlohmann::json json;
  try
  {
    file >> json;
  }
  catch (const std::exception &)
  {
    // a corrupt config must not stop the application from starting at 100%
    return kDefault;
  }

  if (!json.contains("app_settings"))
    return kDefault;

  const nlohmann::json &app_settings = json.at("app_settings");
  if (!app_settings.contains("interface.ui_scale"))
    return kDefault;

  const nlohmann::json &entry = app_settings.at("interface.ui_scale");
  if (!entry.is_number())
    return kDefault; // a string or an object here is not a scale factor

  bool         clean = false;
  const double value = sanitize(entry.get<double>(), &clean);

  if (!clean)
    Logger::log()->warn("ui_scale: persisted interface scale {} is out of the "
                        "supported [{}, {}] range, using {}",
                        entry.dump(),
                        kMin,
                        kMax,
                        value);

  return value;
}

Resolution apply_scale(double configured)
{
  Resolution result;
  result.configured = sanitize(configured);

  // An explicit environment override is the user talking to Qt directly. Do not
  // multiply our factor into it and end up scaled twice -- and do not go on to
  // claim the preference is active, because it is not.
  const QByteArray env = qgetenv("QT_SCALE_FACTOR");

  if (!env.isEmpty())
  {
    bool         parsed = false;
    const double value = env.toDouble(&parsed);

    // Qt ignores a factor it cannot parse or that is not positive, so the
    // process really does run at 1.0 in that case
    const bool usable = parsed && std::isfinite(value) && value > 0.0;

    result.environment_override = true;
    result.effective = usable ? value : kDefault;

    Logger::log()->info("ui_scale: QT_SCALE_FACTOR={} is set in the environment; the "
                        "interface scale setting ({}) is not applied, the process "
                        "runs at {}",
                        env.toStdString(),
                        result.configured,
                        result.effective);

    g_session = result;
    return result;
  }

  result.effective = result.configured;
  g_session = result;

  if (std::abs(result.effective - kDefault) < 1e-6)
    return result;

  // Qt rounds the *screen* DPI factor before our factor is multiplied in, and
  // the default policy on X11/Wayland rounds to a whole number. On a 1.0
  // monitor that is harmless, but the rounded screen factor and the unrounded
  // product then disagree about how many device pixels a logical pixel is, and
  // input coordinates are mapped with one while widgets are laid out with the
  // other -- which is exactly the "cursor lands next to the widget" offset.
  // PassThrough keeps the screen factor unrounded so the two agree again.
  //
  // Set only when we are the ones scaling: a user who pinned the policy
  // themselves keeps it, and at 1.0 we return above without touching anything.
  if (!qEnvironmentVariableIsSet("QT_SCALE_FACTOR_ROUNDING_POLICY"))
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

  // Qt multiplies this into the per-monitor DPI factor rather than replacing
  // it, which is exactly what we want: a 150% monitor stays at 150% and the
  // user's preference rides on top.
  //
  // 'f' with a fixed precision, never 'g': 'g' can emit an exponent form, and
  // the C locale is not guaranteed here -- a comma decimal separator parses as
  // a truncated integer inside Qt, which silently scales by 0 or 9 instead of
  // 0.9.
  QByteArray factor = QByteArray::number(result.effective, 'f', 4);
  factor.replace(',', '.');
  qputenv("QT_SCALE_FACTOR", factor);

  Logger::log()->info("ui_scale: interface scale {} applied via QT_SCALE_FACTOR",
                      result.effective);

  return result;
}

Resolution apply_startup_scale(const char *argv0)
{
  return apply_scale(scale_from_config(startup_config_path(argv0)));
}

Resolution session_resolution() { return g_session; }

double session_scale() { return g_session.effective; }

} // namespace hesiod::ui_scale
