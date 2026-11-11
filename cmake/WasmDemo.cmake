# The web demo: the plugin's DSP as engine.wasm, beside the demo page built from ../webui.
#
# Usage (from the plugin's wasm/CMakeLists.txt, after project() and its cmake/Modules.cmake):
#     include(${SampleRealm-Modules_SOURCE_DIR}/cmake/WasmDemo.cmake)
#     add_wasm_demo(SOURCES Engine.cpp ../Source/dsp/Effect.cpp)
#
# Makes the Engine target and the `ui` test, which runs webui's tests against the built engine.

include_guard(GLOBAL)

# Here, as it does nothing inside a function
enable_testing()

include(${CMAKE_CURRENT_LIST_DIR}/Core.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/WebUi.cmake)

function(add_wasm_demo)
    cmake_parse_arguments(PARSE_ARGV 0 arg "" "" "SOURCES")

    set(pluginCMakeFile "${CMAKE_CURRENT_SOURCE_DIR}/../CMakeLists.txt")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${pluginCMakeFile}")
    file(READ "${pluginCMakeFile}" pluginCMake)

    string(REGEX MATCH "\nproject\\(([A-Za-z0-9_-]+)" _ "${pluginCMake}")
    set(pluginName "${CMAKE_MATCH_1}")

    string(REGEX MATCH "juce_add_plugin[^)]*[ \t\r\n]COMPANY_NAME[ \t\r\n]+(\"[^\"]*\"|[^ \t\r\n)]+)" _ "${pluginCMake}")
    string(REGEX REPLACE "^\"(.*)\"$" "\\1" companyName "${CMAKE_MATCH_1}")

    string(REGEX MATCH "juce_add_plugin[^)]*[ \t\r\n]VERSION[ \t\r\n]+([0-9]+\\.[0-9]+\\.[0-9]+)" _ "${pluginCMake}")
    set(pluginVersion "${CMAKE_MATCH_1}")

    if(NOT pluginName OR NOT companyName OR NOT pluginVersion)
        message(FATAL_ERROR "No project() name, or juce_add_plugin COMPANY_NAME or VERSION, found in ../CMakeLists.txt")
    endif()

    if(NOT companyName MATCHES "^[A-Za-z0-9_-]+$")
        message(FATAL_ERROR "COMPANY_NAME names the demo folder, so it must be one word of letters, digits, - or _, not: ${companyName}")
    endif()

    # The plugin's own JUCE
    string(REGEX MATCH "CPMAddPackage\\([^)]*NAME[ \t\r\n]+JUCE[ \t\r\n][^)]*\\)" jucePackage "${pluginCMake}")
    string(REGEX MATCH "GIT_TAG[ \t\r\n]+([^ \t\r\n)]+)" _ "${jucePackage}")
    set(juceVersion "${CMAKE_MATCH_1}")

    if(NOT juceVersion)
        message(FATAL_ERROR "No CPMAddPackage for JUCE with a GIT_TAG found in ../CMakeLists.txt")
    endif()

    # Otherwise a reconfigure drops the patch
    set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

    # CPM's cache is keyed on the patch's path: named by its content, so JUCE is fetched again
    # only when the patch changes, not with each commit of this repo
    set(patchSource "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/patches/juce-wasm.patch")
    file(SHA256 "${patchSource}" patchHash)
    string(SUBSTRING "${patchHash}" 0 12 patchHash)
    set(jucePatch "${CMAKE_BINARY_DIR}/patches/juce-wasm-${patchHash}.patch")
    configure_file("${patchSource}" "${jucePatch}" COPYONLY)

    # The patch fixes JUCE's wasm support; delete it once JUCE has the fix
    CPMAddPackage(
      NAME
        JUCE
      GIT_TAG
        ${juceVersion}
      GITHUB_REPOSITORY
        juce-framework/JUCE
      OPTIONS
        "JUCE_MODULES_ONLY ON"
      PATCHES
        "${jucePatch}"
    )

    add_executable(Engine ${arg_SOURCES})

    target_include_directories(Engine PRIVATE ../Source)

    add_plugin_core(Engine)

    target_compile_definitions(Engine PRIVATE JUCE_STANDALONE_APPLICATION=1 PLUGIN_VERSION="${pluginVersion}")

    target_link_libraries(Engine PRIVATE juce::juce_audio_basics)

    # A bare .wasm, no Emscripten JS
    target_link_options(Engine PRIVATE --no-entry -sSTANDALONE_WASM)

    set_target_properties(Engine PROPERTIES
        OUTPUT_NAME "engine"
        SUFFIX ".wasm")

    cmake_path(SET uiDir NORMALIZE "${CMAKE_CURRENT_SOURCE_DIR}/../webui")

    # Uploaded as demos/<this folder>/
    set(demoFolder "${companyName}-${pluginName}-${pluginVersion}")
    set(demoDir "${CMAKE_BINARY_DIR}/${demoFolder}")
    # For CI to name its artifact
    file(WRITE "${CMAKE_BINARY_DIR}/demo-folder" "${demoFolder}")

    webui_sources("${uiDir}" "/src/plugin/|/public/plugin/|/web/plugin/" uiInputs npmInstalled)

    add_custom_command(OUTPUT "${demoDir}/index.html"
        COMMAND ${NPM_EXECUTABLE} run typecheck
        COMMAND ${NPM_EXECUTABLE} run build:demo
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${demoDir}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${uiDir}/dist/demo" "${demoDir}"
        WORKING_DIRECTORY "${uiDir}"
        DEPENDS ${uiInputs} "${npmInstalled}"
        VERBATIM)

    add_custom_command(OUTPUT "${demoDir}/engine.wasm"
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:Engine>" "${demoDir}/engine.wasm"
        DEPENDS Engine "${demoDir}/index.html"
        VERBATIM)

    add_custom_target(Demo ALL DEPENDS "${demoDir}/engine.wasm")

    # webui/test/, which plays the built engine too
    add_test(NAME ui
        COMMAND ${NPM_EXECUTABLE} test
        WORKING_DIRECTORY "${uiDir}")
endfunction()
