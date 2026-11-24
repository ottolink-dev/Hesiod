option(HESIOD_ENABLE_TRACY "Enable Tracy CPU zones and process memory sampling" OFF)

add_library(hesiod_tracy INTERFACE)
if(HESIOD_ENABLE_TRACY)
  find_package(Tracy CONFIG REQUIRED)

  # Reject disabled client stubs and mismatched header/library configurations.
  include(CheckCXXSourceCompiles)
  include(CMakePushCheckState)
  cmake_push_check_state(RESET)
  set(CMAKE_REQUIRED_LIBRARIES Tracy::TracyClient)
  unset(HESIOD_TRACY_CLIENT_WORKS CACHE)
  check_cxx_source_compiles("
    #include <tracy/Tracy.hpp>
    #if !defined(TRACY_ENABLE) || !defined(TRACY_ON_DEMAND)
    #error Tracy must enable instrumentation and on-demand capture
    #endif
    int main() { return TracyIsConnected ? 0 : 1; }
  " HESIOD_TRACY_CLIENT_WORKS)
  cmake_pop_check_state()
  if(NOT HESIOD_TRACY_CLIENT_WORKS)
    message(FATAL_ERROR "Use a Tracy client built with TRACY_ENABLE=ON and TRACY_ON_DEMAND=ON")
  endif()

  target_link_libraries(hesiod_tracy INTERFACE Tracy::TracyClient)
  target_compile_definitions(hesiod_tracy INTERFACE HESIOD_ENABLE_TRACY)
endif()
