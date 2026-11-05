#include "hesiod/profiling.hpp"

#if defined(HESIOD_ENABLE_TRACY) && defined(__linux__)
#include <chrono>
#include <cstdio>
#include <sys/resource.h>
#include <unistd.h>

namespace hesiod
{
ProfileMemorySampler::ProfileMemorySampler()
    : worker(
          [](std::stop_token stop)
          {
            tracy::SetThreadName("Hesiod memory sampler");
            TracyPlotConfig("Process RSS", tracy::PlotFormatType::Memory, true, false, 0);
            TracyPlotConfig("Process peak RSS",
                            tracy::PlotFormatType::Memory,
                            true,
                            false,
                            0);
            const long page_size = sysconf(_SC_PAGESIZE);
            while (!stop.stop_requested())
            {
              if (TracyIsConnected)
              {
                if (FILE *file = std::fopen("/proc/self/statm", "r"))
                {
                  unsigned long total_pages = 0, resident_pages = 0;
                  if (std::fscanf(file, "%lu %lu", &total_pages, &resident_pages) == 2 &&
                      page_size > 0)
                    TracyPlot("Process RSS", double(resident_pages) * double(page_size));
                  std::fclose(file);
                }
                rusage usage{};
                if (getrusage(RUSAGE_SELF, &usage) == 0)
                  TracyPlot("Process peak RSS", double(usage.ru_maxrss) * 1024.0);
              }
              std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
          })
{
}
} // namespace hesiod
#endif
