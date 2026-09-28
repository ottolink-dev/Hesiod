project(appimage)

if(NOT Qt6_ROOT AND Qt6_DIR)
    get_filename_component(_qt6_root "${Qt6_DIR}/../../.." ABSOLUTE)
    set(Qt6_ROOT "${_qt6_root}")
endif()

add_custom_target(appimage ALL
    COMMAND ${CMAKE_COMMAND} -E env Qt6_ROOT=${Qt6_ROOT} ./generate_appimage.sh ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating AppImage"
    DEPENDS hesiod
)


