# core/: headers a plugin and its wasm engine both use, as <samplerealm/Transport.h>. They build
# for wasm, so they keep to juce_core and juce_audio_basics.
#
# Usage:
#     include(${SampleRealm-Modules_SOURCE_DIR}/cmake/Core.cmake)
#     add_plugin_core(${PROJECT_NAME})

include_guard(GLOBAL)

function(add_plugin_core target)
    # PUBLIC, so the plugin's tests get it too
    target_include_directories(${target} PUBLIC "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../core")
endfunction()
