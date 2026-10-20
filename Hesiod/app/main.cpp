/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
typedef unsigned int uint;

#include <exception>
#include <new>
#ifdef HESIOD_ENABLE_TRACY
#include <chrono>
#include <cstdlib>
#include <thread>
#endif

#include <QCoreApplication>
#ifdef Q_OS_MACOS
#include <QSurfaceFormat>
#endif

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/app/ui_scale.hpp"
#include "hesiod/cli/batch_mode.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/profiling.hpp"

#if defined(DEBUG_BUILD)
#define HSD_RMODE "Debug"
#elif defined(RELEASE_BUILD)
#define HSD_RMODE "Release"
#else
#define HSD_RMODE "!!! UNDEFINED !!!"
#endif

int main(int argc, char *argv[])
{
#ifdef Q_OS_MACOS
  // macOS needs a core profile selected before QApplication creates any
  // shared contexts. The renderer uses OpenGL 3.3, supported by a 4.1 context.
  QSurfaceFormat format;
  format.setRenderableType(QSurfaceFormat::OpenGL);
  format.setVersion(4, 1);
  format.setProfile(QSurfaceFormat::CoreProfile);
  format.setDepthBufferSize(24);
  format.setStencilBufferSize(8);
  QSurfaceFormat::setDefaultFormat(format);
#endif
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
#ifdef HESIOD_ENABLE_TRACY
    // Batch jobs may finish before Tracy connects. Opt in to a bounded wait.
    if (const char *wait = std::getenv("HESIOD_TRACY_WAIT");
        wait && std::string_view(wait) == "1")
    {
      const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
      while (!TracyIsConnected && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      if (!TracyIsConnected)
      {
        hesiod::Logger::log()->error("No Tracy profiler connected within 30 seconds");
        return 1;
      }
    }
#endif
    [[maybe_unused]] hesiod::ProfileMemorySampler memory_sampler;
    hesiod::HesiodApplication                     app(argc, argv);

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
