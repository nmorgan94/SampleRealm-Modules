# The editor module: the header, presets, overlays and the activation bar every plugin shares.
#
# Usage (from the top-level CMakeLists.txt, AFTER juce_add_plugin, with add_plugin_core):
#     include(${SampleRealm-Modules_SOURCE_DIR}/cmake/Editor.cmake)
#     add_plugin_editor(${PROJECT_NAME})

include_guard(GLOBAL)

# The activation bar is built on it
include(${CMAKE_CURRENT_LIST_DIR}/Licensing.cmake)

cmake_path(GET CMAKE_CURRENT_LIST_DIR PARENT_PATH modulesRepoDir)

juce_add_module("${modulesRepoDir}/editor")

function(add_plugin_editor pluginTarget)
    # Does nothing for a plugin that called it already
    add_licensing(${pluginTarget})

    target_link_libraries(${pluginTarget} PRIVATE editor)
endfunction()
