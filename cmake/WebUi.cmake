# A plugin's webui/: what a build of it reads, and the WebView editor's bundle.
#
# Usage (from the top-level CMakeLists.txt, AFTER juce_add_plugin, for a WebView editor):
#     include(${SampleRealm-Modules_SOURCE_DIR}/cmake/WebUi.cmake)
#     add_webview_ui(${PROJECT_NAME})

include_guard(GLOBAL)

# Gives the files a webui build reads, leaving out those matching `skip`, and the file npm
# writes on every install, with the rule that installs
function(webui_sources uiDir skip inputsVariable installedVariable)
    find_program(NPM_EXECUTABLE npm REQUIRED)

    # The commit npm installs into webui, and here before npm has run
    cmake_path(GET CMAKE_CURRENT_FUNCTION_LIST_DIR PARENT_PATH modulesRepoDir)

    file(GLOB_RECURSE inputs CONFIGURE_DEPENDS
        "${uiDir}/src/*" "${uiDir}/public/*" "${modulesRepoDir}/web/*" "${uiDir}/package*.json" "${uiDir}/vite.config.ts"
        "${uiDir}/../Assets/*")
    list(FILTER inputs EXCLUDE REGEX "${skip}")

    set(installed "${uiDir}/node_modules/.package-lock.json")

    add_custom_command(OUTPUT "${installed}"
        COMMAND ${NPM_EXECUTABLE} ci --no-audit --no-fund
        WORKING_DIRECTORY "${uiDir}"
        DEPENDS "${uiDir}/package.json" "${uiDir}/package-lock.json"
        VERBATIM)

    set(${inputsVariable} "${inputs}" PARENT_SCOPE)
    set(${installedVariable} "${installed}" PARENT_SCOPE)
endfunction()

# Builds webui's plugin page into the plugin, with the WebViewEditor that shows it
function(add_webview_ui pluginTarget)
    set(uiDir "${CMAKE_CURRENT_SOURCE_DIR}/webui")
    set(webUiZip "${CMAKE_CURRENT_BINARY_DIR}/WebUI.zip")

    webui_sources("${uiDir}" "/src/demo/|/public/demo/|/web/(demo|testing)/" uiInputs npmInstalled)

    add_custom_command(OUTPUT "${webUiZip}"
        COMMAND ${NPM_EXECUTABLE} run typecheck
        COMMAND ${NPM_EXECUTABLE} run build:plugin
        COMMAND ${CMAKE_COMMAND} -E chdir "${uiDir}/dist/plugin"
                ${CMAKE_COMMAND} -E tar cf "${webUiZip}" --format=zip .
        WORKING_DIRECTORY "${uiDir}"
        DEPENDS ${uiInputs} "${npmInstalled}"
        VERBATIM)

    juce_add_binary_data(${pluginTarget}_WebUI
        HEADER_NAME "WebUIData.h"
        NAMESPACE WebUIData
        SOURCES "${webUiZip}")

    # webview_editor is built on the editor module
    include(${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Editor.cmake)

    if(NOT TARGET webview_editor)
        cmake_path(GET CMAKE_CURRENT_FUNCTION_LIST_DIR PARENT_PATH modulesRepoDir)
        juce_add_module("${modulesRepoDir}/webview_editor")
    endif()

    # For a plugin whose native editor needs only add_plugin_controls
    add_plugin_editor(${pluginTarget})

    target_link_libraries(${pluginTarget} PRIVATE ${pluginTarget}_WebUI webview_editor)
endfunction()
