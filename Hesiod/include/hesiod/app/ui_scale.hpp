/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <string>

/** @file ui_scale.hpp
 *  @brief Application-wide interface scaling.
 *
 *  Scaling is applied once, before QApplication exists, by handing the factor
 *  to Qt itself (QT_SCALE_FACTOR). Qt then multiplies it into the per-monitor
 *  device pixel ratio, so a 150% monitor at scale 1.25 renders at 1.875 rather
 *  than either factor being lost or applied twice, and every logical metric --
 *  point-sized fonts, stylesheet px, fixed widths, icons, layout spacing --
 *  follows without any widget knowing about it.
 *
 *  Deliberately *not* implemented by walking widgets and multiplying their
 *  metrics: that mutates the metrics in place, so a second change compounds on
 *  the first, and it silently misses every fixed size set in a constructor.
 *  Here the baseline is the source code's own logical values, which are never
 *  touched, and the factor is an absolute number read fresh at startup. Two
 *  changes in a row therefore give the same result as jumping straight to the
 *  second one.
 *
 *  The cost is that a change lands on restart. Qt cannot re-run high-DPI
 *  scaling on a live application, and the alternatives that do work live (font
 *  substitution plus a stylesheet rewrite) leave the hard-coded widget sizes
 *  behind and look broken at anything past ~1.3.
 */

namespace hesiod::ui_scale
{

inline constexpr double kMin = 0.5;
inline constexpr double kMax = 3.0;
inline constexpr double kDefault = 1.0;
inline constexpr double kStep = 0.05;

/** @brief Make a persisted or user-entered factor safe to use.
 *
 * Non-finite values (NaN, +/-inf) and anything that is not a number fall back
 * to @ref kDefault; a finite value outside the supported window is clamped into
 * it. `out_was_clean` reports whether the input was already usable as-is, which
 * is what lets the loader log a corrected config instead of silently changing
 * the user's setting.
 */
double sanitize(double value, bool *out_was_clean = nullptr);

/** @brief Directory holding the running executable, before QApplication exists.
 *
 * On Windows this asks the loader (GetModuleFileNameW) rather than trusting
 * argv[0]: a process started through PATH, a shortcut or CreateProcess can be
 * handed a bare name or a relative path, which would resolve the portable
 * config against whatever directory the launcher happened to be in. @p argv0 is
 * only the fallback.
 */
std::string executable_dir(const char *argv0);

/** @brief Config file path, resolved without a QCoreApplication instance.
 *
 * Mirrors get_config_file_path_auto("hesiod"): a hesiod.json or portable.flag
 * next to the executable wins, otherwise the per-user config location. Needed
 * because the scale has to be known before QApplication is constructed, and
 * QCoreApplication::applicationDirPath() is not available that early.
 */
std::string startup_config_path(const char *argv0);

/// Scale stored in @p config_path, sanitized. Missing file, malformed JSON or a
/// missing/ill-typed key all yield @ref kDefault.
double scale_from_config(const std::string &config_path);

/** @brief What the settings asked for versus what the process actually runs at.
 *
 * The two differ when QT_SCALE_FACTOR is already set in the environment: Qt
 * obeys that, the preference is not applied, and reporting the preference as if
 * it were active is how a user ends up believing the setting is broken.
 */
struct Resolution
{
  double configured = kDefault; ///< the persisted preference, sanitized
  double effective = kDefault;  ///< what Qt will actually scale by
  bool   environment_override = false; ///< QT_SCALE_FACTOR came from outside
};

/** @brief Hand @p configured to Qt, unless the environment already spoke.
 *
 * Must run before QApplication is constructed. Records the outcome for
 * session_resolution(). Split from the config read so the decision is testable
 * without touching a settings file.
 */
Resolution apply_scale(double configured);

/// scale_from_config(startup_config_path(argv0)) fed to apply_scale().
Resolution apply_startup_scale(const char *argv0);

/// What apply_scale() decided, or an unapplied default if it never ran.
Resolution session_resolution();

/// The factor this process actually runs at, 1.0 if apply_scale() never ran.
double session_scale();

} // namespace hesiod::ui_scale
