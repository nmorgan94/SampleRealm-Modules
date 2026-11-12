# The activation module: the activation bar, on the editor and licensing modules. Only for a plugin
# that asks for a licence; without it a plugin has no licensing code.
#
# Usage (from the top-level CMakeLists.txt, AFTER juce_add_plugin, with add_plugin_core):
#     include(${SampleRealm-Modules_SOURCE_DIR}/cmake/Activation.cmake)
#     add_plugin_activation(${PROJECT_NAME})

include_guard(GLOBAL)

include(${CMAKE_CURRENT_LIST_DIR}/Licensing.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/Editor.cmake)

cmake_path(GET CMAKE_CURRENT_LIST_DIR PARENT_PATH modulesRepoDir)

juce_add_module("${modulesRepoDir}/activation")

function(add_plugin_activation pluginTarget)
    # Does nothing for a plugin that called it already
    add_licensing(${pluginTarget})

    add_plugin_editor(${pluginTarget})

    target_link_libraries(${pluginTarget} PRIVATE activation)
endfunction()
