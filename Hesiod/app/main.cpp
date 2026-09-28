/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
typedef unsigned int uint;

#include <exception>
#include <new>

#include <QCoreApplication>

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/app/ui_scale.hpp"
#include "hesiod/cli/batch_mode.hpp"
#include "hesiod/logger.hpp"

#if defined(DEBUG_BUILD)
#define HSD_RMODE "Debug"
#elif defined(RELEASE_BUILD)
#define HSD_RMODE "Release"
#else
#define HSD_RMODE "!!! UNDEFINED !!!"
#endif

int main(int argc, char *argv[])
{
  hesiod::Logger::log()->info("Welcome to Hesiod v{}.{}.{}!",
                              HESIOD_VERSION_MAJOR,
                              HESIOD_VERSION_MINOR,
                              HESIOD_VERSION_PATCH);

  hesiod::Logger::log()->info("Release mode: {}", std::string(HSD_RMODE));

  // --- Start app

  qputenv("QT_LOGGING_RULES", HESIOD_QPUTENV_QT_LOGGING_RULES);

  qputenv("QTWEBENGINE_CHROMIUM_FLAGS",
          "--ignore-gpu-blocklist "
          "--enable-webgl "
          "--disable-gpu-driver-bug-workarounds");

  // The interface scale has to reach Qt before QApplication is constructed:
  // high-DPI scaling is resolved once, at that point. Pinning the application
  // name first makes QStandardPaths give the same config directory the running
  // application will use, which it otherwise only derives from argv[0] once an
  // instance exists.
  QCoreApplication::setApplicationName("hesiod");
  hesiod::ui_scale::apply_startup_scale(argc > 0 ? argv[0] : nullptr);

  // Backstop only: anything that reaches here has already escaped the guards
  // around graph updates. Reporting it and exiting non-zero beats std::terminate
  // calling abort(), which produces a bare fast-fail with no explanation.
  try
  {
    hesiod::HesiodApplication app(argc, argv);

    if (!app.is_headless())
    {
      app.show();
      return app.exec();
    }

    return app.get_exit_code();
  }
  catch (const std::bad_alloc &)
  {
    hesiod::Logger::log()->critical("Fatal: out of memory, Hesiod is shutting down");
    return 1;
  }
  catch (const std::exception &e)
  {
    hesiod::Logger::log()->critical("Fatal: unhandled exception, Hesiod is shutting "
                                    "down: {}",
                                    e.what());
    return 1;
  }
  catch (...)
  {
    hesiod::Logger::log()->critical("Fatal: unhandled unknown exception, Hesiod is "
                                    "shutting down");
    return 1;
  }
}
