project(hesiod LANGUAGES CXX)

# ------------------------------
# Qt6 setup
# ------------------------------
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOMOC_VERBOSE ON)

# ------------------------------
# Source files
# ------------------------------
file(GLOB_RECURSE HESIOD_GUI_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp)
file(GLOB_RECURSE HESIOD_SOURCES CONFIGURE_DEPENDS
     ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

if(HESIOD_MINIMAL_NODE_SET)
  # option for a minimal set of nodes for quick compile time when tempering with
  # node architecture
  set(NODES_FCT_DIR
      "${CMAKE_CURRENT_SOURCE_DIR}/src/model/nodes/nodes_function")

  file(GLOB ALL_NODE_SOURCES "${NODES_FCT_DIR}/*.cpp")
  set(MINIMAL_NODE_SET_SOURCES
      "${NODES_FCT_DIR}/blender_bridge.cpp"
      "${NODES_FCT_DIR}/broadcast.cpp"
      "${NODES_FCT_DIR}/debug.cpp"
      "${NODES_FCT_DIR}/flooding_uniform_level.cpp"
      "${NODES_FCT_DIR}/gabor_wave_fbm.cpp"
      "${NODES_FCT_DIR}/gain.cpp"
      "${NODES_FCT_DIR}/hydraulic_stream_log.cpp"
      "${NODES_FCT_DIR}/export_heightmap.cpp"
      "${NODES_FCT_DIR}/noise.cpp"
      "${NODES_FCT_DIR}/preview.cpp"
      "${NODES_FCT_DIR}/receive.cpp"
      "${NODES_FCT_DIR}/smooth_cpulse.cpp"
      "${NODES_FCT_DIR}/thru.cpp"
      "${NODES_FCT_DIR}/toggle.cpp")

  set(COMPLENTARY_NODE_SOURCES ${ALL_NODE_SOURCES})
  foreach(f ${MINIMAL_NODE_SET_SOURCES})
    list(REMOVE_ITEM COMPLENTARY_NODE_SOURCES ${f})
  endforeach()

  foreach(f ${COMPLENTARY_NODE_SOURCES})
    list(REMOVE_ITEM HESIOD_SOURCES ${f})
  endforeach()
endif()

# ------------------------------
# Executable
# ------------------------------
# Share the application implementation with integration tests; compile it once.
add_library(hesiod_core OBJECT ${HESIOD_SOURCES} ${HESIOD_GUI_INCLUDES})
add_executable(${PROJECT_NAME} app/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE hesiod_core)

# ------------------------------
# Include directories
# ------------------------------
target_include_directories(
  hesiod_core
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_BINARY_DIR}/include
         ${OPENGL_INCLUDE_DIRS} ${GLEW_INCLUDE_DIRS} ${GLUT_INCLUDE_DIRS})

# ------------------------------
# Compiler features
# ------------------------------
target_compile_features(hesiod_core PUBLIC cxx_std_20)

# ------------------------------
# Link libraries
# ------------------------------
target_link_libraries(
  hesiod_core
  PUBLIC hesiod_options
         hesiod_platform
         hesiod_qt_logging
         args
         spdlog::spdlog
         nlohmann_json::nlohmann_json
         highmap
         gnode
         gnodegui
         meta
         meta_qt
         Qt6::Core
         Qt6::OpenGL
         Qt6::Widgets
         Qt6::OpenGLWidgets
         Qt6::WebEngineWidgets
         qterrain-renderer
         qtexture_downloader
         ZLIB::ZLIB)

# ------------------------------
# Precompiled Headers (PCH)
# ------------------------------
if(HESIOD_ENABLE_PCH)
  message(STATUS "Precompiled Headers (PCH) enabled for target hesiod_core")
  target_precompile_headers(
    hesiod_core
    PRIVATE
    <vector>
    <string>
    <memory>
    <cmath>
    <functional>
    <algorithm>
    <map>
    <unordered_map>
    <iostream>
    <spdlog/spdlog.h>
    <nlohmann/json.hpp>
    <QString>
    <QWidget>
    <QObject>
    <QPainter>
    <QPoint>
    <QColor>)
endif()

# ------------------------------
# Copy data folder
# ------------------------------
file(COPY ${CMAKE_SOURCE_DIR}/Hesiod/data
     DESTINATION ${EXECUTABLE_OUTPUT_PATH}/.)

file(COPY ${CMAKE_SOURCE_DIR}/Hesiod/data
     DESTINATION ${EXECUTABLE_OUTPUT_PATH}/../.)

# ------------------------------
# Build information messages
# ------------------------------
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

if(HESIOD_ENABLE_TESTS)
  find_package(Qt6 REQUIRED COMPONENTS Test)
  add_library(test_model_headers OBJECT
              ${CMAKE_SOURCE_DIR}/tests/model/model_headers.cpp)
  set_target_properties(test_model_headers PROPERTIES AUTOMOC OFF)
  target_include_directories(test_model_headers
                             PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_link_libraries(test_model_headers PRIVATE gnode highmap meta)
  target_compile_features(test_model_headers PRIVATE cxx_std_20)

  add_executable(test_graph_editor
                 ${CMAKE_SOURCE_DIR}/tests/gui/test_graph_editor.cpp)
  add_dependencies(test_graph_editor test_model_headers)
  target_link_libraries(test_graph_editor PRIVATE hesiod_core Qt6::Test)
  add_test(NAME graph_editor COMMAND test_graph_editor)
  set_tests_properties(
    graph_editor PROPERTIES WORKING_DIRECTORY ${EXECUTABLE_OUTPUT_PATH}
                            ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_executable(test_autosave ${CMAKE_SOURCE_DIR}/tests/gui/test_autosave.cpp)
  add_dependencies(test_autosave test_model_headers)
  target_link_libraries(test_autosave PRIVATE hesiod_core Qt6::Test)
  add_test(NAME autosave COMMAND test_autosave)
  set_tests_properties(
    autosave PROPERTIES WORKING_DIRECTORY ${EXECUTABLE_OUTPUT_PATH}
                        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
