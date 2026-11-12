# The editor module: the header, presets and overlays. The activation bar is add_plugin_activation's.
#
# Usage (from the top-level CMakeLists.txt, AFTER juce_add_plugin, with add_plugin_core):
#     include(${SampleRealm-Modules_SOURCE_DIR}/cmake/Editor.cmake)
#     add_plugin_editor(${PROJECT_NAME})

include_guard(GLOBAL)

# The header's meter is in controls
include(${CMAKE_CURRENT_LIST_DIR}/Controls.cmake)

cmake_path(GET CMAKE_CURRENT_LIST_DIR PARENT_PATH modulesRepoDir)

juce_add_module("${modulesRepoDir}/editor")

function(add_plugin_editor pluginTarget)
    add_plugin_controls(${pluginTarget})

    target_link_libraries(${pluginTarget} PRIVATE editor)
endfunction()
