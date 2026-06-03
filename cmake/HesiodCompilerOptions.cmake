add_library(hesiod_options INTERFACE)

# Require C++20
target_compile_features(hesiod_options INTERFACE cxx_std_20)

add_definitions(-D_USE_MATH_DEFINES)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  message(STATUS "GNU|Clang compiler")

  # Common flags
  target_compile_options(
    hesiod_options
    INTERFACE -Ofast -ffast-math -Wall -Wextra -Wno-dangling-reference
              -Wno-deprecated-declarations)

  # Debug flags
  target_compile_options(
    hesiod_options
    INTERFACE $<$<CONFIG:Debug>:
              -g
              -DLOG_LEVEL=3
              -fno-omit-frame-pointer
              -Wpedantic
              -Wno-psabi
              -Wno-unused-parameter
              -Wcast-align
              -Wcast-qual
              -Wnull-dereference
              -Wnon-virtual-dtor
              -Woverloaded-virtual
              -Wzero-as-null-pointer-constant
              -Wduplicated-cond
              -Wduplicated-branches
              -Wlogical-op
              -Wuseless-cast
              $<$<BOOL:${HESIOD_UNUSED_FUNCTIONS}>:-Wunused-function>
              >)

  # Release flags
  target_compile_options(hesiod_options
                         INTERFACE $<$<CONFIG:Release>:-DLOG_LEVEL=3>)

  # LTO and function sections (optional)
  if(HESIOD_ENABLE_LTO)
    message(STATUS "HESIOD_ENABLE_LTO is enabled")
    target_compile_options(hesiod_options INTERFACE -ffunction-sections
                                                    -fdata-sections -flto)
    target_link_options(hesiod_options INTERFACE -Wl,--gc-sections
                        -Wl,--print-gc-sections)
  endif()

  if(HESIOD_MINIMAL_NODE_SET)
    message(STATUS "HESIOD_MINIMAL_NODE_SET is enabled")
    target_compile_definitions(hesiod_options INTERFACE HESIOD_MINIMAL_NODE_SET)
  endif()
endif()

# MSVC options
if(MSVC)
  message(STATUS "MSVC compiler")
  target_compile_options(hesiod_options INTERFACE /W4)
endif()
