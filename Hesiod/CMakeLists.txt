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
file(GLOB_RECURSE HESIOD_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/app/main.cpp)

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
add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE ${HESIOD_SOURCES} ${HESIOD_GUI_INCLUDES})

# ------------------------------
# Include directories
# ------------------------------
target_include_directories(
  ${PROJECT_NAME}
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_BINARY_DIR}/include
          ${OPENGL_INCLUDE_DIRS} ${GLEW_INCLUDE_DIRS} ${GLUT_INCLUDE_DIRS})

# ------------------------------
# Compiler features
# ------------------------------
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

# ------------------------------
# Link libraries
# ------------------------------
target_link_libraries(
  ${PROJECT_NAME}
  PRIVATE hesiod_options
          hesiod_platform
          hesiod_qt_logging
          args
          spdlog::spdlog
          nlohmann_json::nlohmann_json
          highmap
          gnode
          gnodegui
          attributes
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
