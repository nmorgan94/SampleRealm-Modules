cmake_minimum_required(VERSION 3.25)

# Builds this repo's own tests. A plugin fetches the repo and includes cmake/*.cmake instead.

set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "" FORCE)

project(SampleRealmModules)

enable_testing()

set(CMAKE_CXX_STANDARD 23)

set(CPM_DOWNLOAD_VERSION 0.43.1)
set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")

if(NOT EXISTS ${CPM_DOWNLOAD_LOCATION})
    message(STATUS "Downloading CPM.cmake to ${CPM_DOWNLOAD_LOCATION}")
    file(DOWNLOAD https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake ${CPM_DOWNLOAD_LOCATION})
endif()

include(${CPM_DOWNLOAD_LOCATION})

CPMAddPackage(
    NAME
      JUCE
    GIT_TAG
      9.0.2
    GITHUB_REPOSITORY
      juce-framework/JUCE
)

# For Catch2, as a plugin's tests get it
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Tests.cmake)

# The company and the product name the salt and the licence file, as a plugin's do
juce_add_console_app(ModulesTests COMPANY_NAME Example VERSION 0.0.1)

# A made-up key, so the generated key table isn't empty
set(ENV{LICENCE_KEYS} "00000000000000aa:00000000000000000000000000000000000000000000000000000000000000aa")

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Licensing.cmake)
add_licensing(ModulesTests)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Core.cmake)
add_plugin_core(ModulesTests)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Activation.cmake)
add_plugin_activation(ModulesTests)

# Tests/plugin comes with licensing::fixtures
file(GLOB TestFiles CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Tests/*.cpp")

target_sources(ModulesTests PRIVATE ${TestFiles})

function(configure_test_app target)
    # A plugin has this from JUCE
    target_compile_definitions(${target} PRIVATE
        JucePlugin_Manufacturer="Example"
        JucePlugin_VersionString="0.0.1"
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0)

    target_link_libraries(${target} PRIVATE
        Catch2::Catch2WithMain
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
endfunction()

configure_test_app(ModulesTests)
target_link_libraries(ModulesTests PRIVATE licensing::fixtures juce::juce_audio_basics)

catch_discover_tests(ModulesTests TEST_PREFIX "modules.unit.")

# The editor as a plugin without activation has it: no licensing module
juce_add_console_app(ModulesEditorOnlyTests COMPANY_NAME Example VERSION 0.0.1)

add_plugin_core(ModulesEditorOnlyTests)
add_plugin_editor(ModulesEditorOnlyTests)

target_sources(ModulesEditorOnlyTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Tests/editor-only/EditorOnlyTests.cpp")
configure_test_app(ModulesEditorOnlyTests)

catch_discover_tests(ModulesEditorOnlyTests TEST_PREFIX "modules.unit.editor-only.")
