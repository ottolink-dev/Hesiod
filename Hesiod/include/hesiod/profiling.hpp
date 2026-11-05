#pragma once

#ifdef HESIOD_ENABLE_TRACY
#include <string_view>
#include <tracy/Tracy.hpp>

#define HSD_PROFILE_ZONE(name) ZoneScopedN(name)
// Evaluate text only during capture, and keep temporaries alive while Tracy copies it.
#define HSD_PROFILE_TEXT(text)                                                           \
  do                                                                                     \
  {                                                                                      \
    if (TracyIsConnected)                                                                \
    {                                                                                    \
      const auto            &hsd_profile_text = (text);                                  \
      const std::string_view hsd_view(hsd_profile_text);                                 \
      ZoneText(hsd_view.data(), hsd_view.size());                                        \
    }                                                                                    \
  } while (false)
#else
#define HSD_PROFILE_ZONE(name)
#define HSD_PROFILE_TEXT(text)                                                           \
  do                                                                                     \
  {                                                                                      \
  } while (false)
#endif

#if defined(HESIOD_ENABLE_TRACY) && defined(__linux__)
#include <thread>

namespace hesiod
{
// Process-wide samples, including allocations made inside external libraries.
class ProfileMemorySampler
{
public:
  ProfileMemorySampler();

private:
  std::jthread worker;
};
} // namespace hesiod
#else
namespace hesiod
{
class ProfileMemorySampler
{
};
} // namespace hesiod
#endif
