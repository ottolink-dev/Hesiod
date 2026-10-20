# FastNoiseSIMD (MIT, https://github.com/Auburn/FastNoiseSIMD, commit 19dc220)
#
# Used by the MountainMassif node, whose ridge and warp stages are tuned against
# this exact noise implementation. Each SIMD level
# lives in its own translation unit and needs its own code generation flags;
# the runtime picks the best level supported by the CPU.
project(fastnoise_simd LANGUAGES CXX)

set(FNS_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(FNS_SOURCES ${FNS_DIR}/FastNoiseSIMD.cpp
                ${FNS_DIR}/FastNoiseSIMD_internal.cpp)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64|ARM64)$")
  list(APPEND FNS_SOURCES ${FNS_DIR}/FastNoiseSIMD_neon.cpp)
else()
  list(APPEND FNS_SOURCES ${FNS_DIR}/FastNoiseSIMD_sse2.cpp
       ${FNS_DIR}/FastNoiseSIMD_sse41.cpp ${FNS_DIR}/FastNoiseSIMD_avx2.cpp)

  if(MSVC)
    set_source_files_properties(${FNS_DIR}/FastNoiseSIMD_avx2.cpp
                                PROPERTIES COMPILE_OPTIONS "/arch:AVX2")
  else()
    set_source_files_properties(${FNS_DIR}/FastNoiseSIMD_sse2.cpp
                                PROPERTIES COMPILE_OPTIONS "-msse2")
    set_source_files_properties(${FNS_DIR}/FastNoiseSIMD_sse41.cpp
                                PROPERTIES COMPILE_OPTIONS "-msse4.1")
    set_source_files_properties(${FNS_DIR}/FastNoiseSIMD_avx2.cpp
                                PROPERTIES COMPILE_OPTIONS "-mavx2;-mfma")
  endif()
endif()

add_library(fastnoise_simd STATIC ${FNS_SOURCES})
target_include_directories(fastnoise_simd PUBLIC ${FNS_DIR})
set_target_properties(fastnoise_simd PROPERTIES POSITION_INDEPENDENT_CODE ON)

# third-party code, keep the build log clean
if(MSVC)
  target_compile_options(fastnoise_simd PRIVATE /W0)
else()
  target_compile_options(fastnoise_simd PRIVATE -w)
endif()
