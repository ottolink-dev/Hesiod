add_custom_target(
  hesiod_streamer_zip
  COMMAND ${CMAKE_COMMAND} -E rm -rf "${CMAKE_CURRENT_SOURCE_DIR}/hesiod_streamer/__pycache__"
  COMMAND ${CMAKE_COMMAND} -E rm -f "${CMAKE_CURRENT_SOURCE_DIR}/hesiod_streamer.zip"
  COMMAND ${CMAKE_COMMAND} -E tar cf "${CMAKE_CURRENT_SOURCE_DIR}/hesiod_streamer.zip" --format=zip hesiod_streamer
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "Generating Blender bridge add-on zip: hesiod_streamer.zip"
)

add_custom_target(
  blender_bridge_zip
  DEPENDS hesiod_streamer_zip
)
