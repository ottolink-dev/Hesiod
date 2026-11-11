project(external LANGUAGES CXX)

# args
include(args.cmake)

# GNode
set(GNODE_ENABLE_DOCS OFF)
set(GNODE_ENABLE_EXAMPLES OFF)
set(GNODE_ENABLE_TESTS OFF)
add_subdirectory(GNode)

# GNode GUI
set(GNODEGUI_ENABLE_TESTS OFF)
add_subdirectory(GNodeGUI)

# HighMap
# FetchContent uses the first declaration, so pin LightUSD before HighMap
# declares its moving main branch. Keep this revision aligned with
# nix/highmap-sources.nix; newer LightUSD defaults to an incompatible API.
include(FetchContent)
FetchContent_Declare(
  LightUSD
  GIT_REPOSITORY https://github.com/lighttransport/LightUSD.git
  GIT_TAG 1fdd60a6171724b5ce0e6716c25b2bba1aed7ad3)

set(HIGHMAP_ENABLE_DOCS OFF)
set(HIGHMAP_ENABLE_EXAMPLES OFF)
set(HIGHMAP_ENABLE_TESTS OFF)
set(HIGHMAP_ENABLE_BENCHMARKS OFF)
if(HESIOD_PROFILE_BUILD)
  set(HIGHMAP_ENABLE_PROFILE_BUILD ON)
endif()
if(HESIOD_ENABLE_PCH)
  set(HIGHMAP_ENABLE_PCH ON)
endif()
add_subdirectory(HighMap)
if(APPLE)
  target_include_directories(clwrapper PUBLIC ${HESIOD_OPENCL_INCLUDE_DIR})
endif()

# Meta (transitional: built alongside Attributes)
set(META_ENABLE_TESTS OFF)
set(META_ENABLE_GLM_TYPES ON)
set(META_ENABLE_COLOR_GRADIENT_TYPES ON)
set(META_ENABLE_QT_UI ON)
set(META_ENABLE_FTXUI_UI OFF)
add_subdirectory(Meta)

# QTerrainRenderer
add_subdirectory(QTerrainRenderer)

# QTextureDownloader
add_subdirectory(QTextureDownloader)
