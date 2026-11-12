# The controls module: the parameter layout, the labelled slider and the level meter. All a plugin
# without the header, presets or activation needs; add_plugin_editor brings it in for the rest.
#
# Usage (from the top-level CMakeLists.txt, AFTER juce_add_plugin, with add_plugin_core):
#     include(${SampleRealm-Modules_SOURCE_DIR}/cmake/Controls.cmake)
#     add_plugin_controls(${PROJECT_NAME})

include_guard(GLOBAL)

cmake_path(GET CMAKE_CURRENT_LIST_DIR PARENT_PATH modulesRepoDir)

juce_add_module("${modulesRepoDir}/controls")

function(add_plugin_controls pluginTarget)
    target_link_libraries(${pluginTarget} PRIVATE controls)
endfunction()
