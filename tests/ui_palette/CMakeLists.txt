# Regression tests for the interface scale and the node palette sidebar.
#
# Deliberately built from the handful of sources under test rather than by
# linking the application: both live behind plain value types (no HesiodApplication,
# no OpenCL, no node registry), so the test binary compiles in seconds and the
# tests cannot accidentally depend on a running application context.

set(TARGET hesiod_ui_palette_test)

set(HESIOD_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../Hesiod)

add_executable(
  ${TARGET}
  main.cpp
  ${HESIOD_SRC}/src/app/ui_scale.cpp
  ${HESIOD_SRC}/src/gui/widgets/node_palette_sidebar.cpp
  ${HESIOD_SRC}/src/gui/widgets/scrollable_dialog.cpp
  ${HESIOD_SRC}/src/logger.cpp
  ${HESIOD_SRC}/include/hesiod/gui/widgets/node_palette_sidebar.hpp
  ${HESIOD_SRC}/include/hesiod/gui/widgets/scrollable_dialog.hpp)

set_target_properties(${TARGET} PROPERTIES AUTOMOC ON)

target_include_directories(${TARGET} PRIVATE ${HESIOD_SRC}/include)

find_package(Qt6 REQUIRED COMPONENTS Test)

target_link_libraries(${TARGET} PRIVATE Qt6::Core Qt6::Widgets Qt6::Test
                                        spdlog::spdlog nlohmann_json::nlohmann_json)

target_compile_features(${TARGET} PRIVATE cxx_std_20)
