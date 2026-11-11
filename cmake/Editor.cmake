# The editor module: the header, presets, overlays and the activation bar every plugin shares.
#
# Usage (from the top-level CMakeLists.txt, AFTER juce_add_plugin, with add_plugin_core and add_licensing):
#     include(${SampleRealm-Modules_SOURCE_DIR}/cmake/Editor.cmake)
#     add_plugin_editor(${PROJECT_NAME})

include_guard(GLOBAL)

cmake_path(GET CMAKE_CURRENT_LIST_DIR PARENT_PATH modulesRepoDir)

juce_add_module("${modulesRepoDir}/editor")

function(add_plugin_editor pluginTarget)
    target_link_libraries(${pluginTarget} PRIVATE editor)
endfunction()
