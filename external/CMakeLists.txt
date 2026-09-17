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
