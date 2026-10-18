add_library(hesiod_platform INTERFACE)

# Linux
if(UNIX AND NOT APPLE)
  message(STATUS "Platform: Linux")
  target_compile_definitions(hesiod_platform INTERFACE HSD_OS_LINUX)

  # Windows
elseif(WIN32)
  message(STATUS "Platform: Windows")

elseif(APPLE)
  message(STATUS "Platform: macOS (experimental)")
  target_compile_definitions(hesiod_platform INTERFACE HSD_OS_MACOS)

  # Apple's compiler needs the separately installed OpenMP runtime. HighMap
  # calls find_package(OpenMP), so supply its standard discovery hints here.
  if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    find_path(HESIOD_OPENMP_INCLUDE_DIR omp.h
              HINTS /opt/homebrew/opt/libomp/include
                    /usr/local/opt/libomp/include REQUIRED)
    find_library(
      HESIOD_OPENMP_LIBRARY omp HINTS /opt/homebrew/opt/libomp/lib
                                      /usr/local/opt/libomp/lib REQUIRED)
    foreach(lang C CXX)
      set(OpenMP_${lang}_FLAGS
          "-Xpreprocessor -fopenmp"
          CACHE STRING "OpenMP flags")
      set(OpenMP_${lang}_INCLUDE_DIR
          "${HESIOD_OPENMP_INCLUDE_DIR}"
          CACHE PATH "OpenMP headers")
      set(OpenMP_${lang}_LIB_NAMES
          "omp"
          CACHE STRING "OpenMP libraries")
    endforeach()
    set(OpenMP_omp_LIBRARY
        "${HESIOD_OPENMP_LIBRARY}"
        CACHE FILEPATH "OpenMP runtime")
  endif()

  # CLWrapper uses the Khronos C++ headers; the SDK only provides the C API.
  find_path(
    HESIOD_OPENCL_INCLUDE_DIR CL/opencl.hpp
    HINTS /opt/homebrew/opt/opencl-clhpp-headers/include
          /usr/local/opt/opencl-clhpp-headers/include /opt/homebrew/include
          /usr/local/include REQUIRED)
  target_include_directories(hesiod_platform
                             INTERFACE ${HESIOD_OPENCL_INCLUDE_DIR})

  # Unsupported platforms
else()
  message(
    FATAL_ERROR
      "Unsupported platform. Only Linux, Windows and macOS are supported.")
endif()
