# Create a small interface target that provides the git version file
add_library(hesiod_git_version INTERFACE)

# --- Get Git tag -------------------------------------------------------------

find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --dirty --always --match "NOT_A_TAG"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE HESIOD_GIT_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()

if(HESIOD_GIT_COMMIT)
    set(HESIOD_GIT_TAG "v${PROJECT_VERSION}-${HESIOD_GIT_COMMIT}")
else()
    set(HESIOD_GIT_TAG "v${PROJECT_VERSION}")
endif()

# --- Generate text file in build tree ----------------------------------------

set(HESIOD_GIT_VERSION_FILE "${CMAKE_BINARY_DIR}/bin/data/git_version.txt")

# Use configure_file so it rewrites only if changed (no recompilation)
configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/git_version.in"
    "${HESIOD_GIT_VERSION_FILE}"
    @ONLY
)

# make a copy
file(COPY "${HESIOD_GIT_VERSION_FILE}"
     DESTINATION "${CMAKE_BINARY_DIR}/data/.")

# Make the file path available to consumers
target_compile_definitions(
    hesiod_git_version
    INTERFACE
        HESIOD_GIT_VERSION_FILE=\"${HESIOD_GIT_VERSION_FILE}\"
)
