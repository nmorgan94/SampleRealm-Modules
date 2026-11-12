# Offline licence verification: the licensing module and the Monocypher it verifies with.
#
# Usage (from the top-level CMakeLists.txt, AFTER juce_add_plugin):
#     include(${SampleRealm-Modules_SOURCE_DIR}/cmake/Licensing.cmake)
#     add_licensing(${PROJECT_NAME})
#
# The plugin's tests link licensing::fixtures for LicenceFixtures.h, the test licences, and the
# tests of the plugin's own company name, product name and keys.
#
# A plugin that never asks for activation doesn't call it, and has no licensing code. One that
# does calls add_plugin_activation, which calls this.

include_guard(GLOBAL)

# Release 4.0.3, by commit so a moved tag can't change it
CPMAddPackage(
    NAME
      monocypher
    GIT_TAG
      ab2b16dd619ad5f6979a4fbe69cfa324a6fcc35f
    GITHUB_REPOSITORY
      LoupVaillant/Monocypher
    DOWNLOAD_ONLY
      YES
)

add_library(monocypher STATIC EXCLUDE_FROM_ALL
    "${monocypher_SOURCE_DIR}/src/monocypher.c"
    "${monocypher_SOURCE_DIR}/src/optional/monocypher-ed25519.c")

target_include_directories(monocypher SYSTEM PUBLIC
    "${monocypher_SOURCE_DIR}/src"
    "${monocypher_SOURCE_DIR}/src/optional")

set_target_properties(monocypher PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    C_VISIBILITY_PRESET hidden
    FOLDER "Dependencies")

cmake_path(GET CMAKE_CURRENT_LIST_DIR PARENT_PATH modulesRepoDir)

juce_add_module("${modulesRepoDir}/licensing")
target_link_libraries(licensing INTERFACE monocypher)

function(add_licensing pluginTarget)
    # add_plugin_activation calls this too
    get_target_property(alreadyAdded ${pluginTarget} SAMPLEREALM_LICENSING_ADDED)

    if(alreadyAdded)
        return()
    endif()

    set_target_properties(${pluginTarget} PROPERTIES SAMPLEREALM_LICENSING_ADDED TRUE)

    cmake_path(GET CMAKE_CURRENT_FUNCTION_LIST_DIR PARENT_PATH modulesRepoDir)

    string(STRIP "$ENV{ACTIVATION_URL}" activationUrlFromEnvironment)

    if(activationUrlFromEnvironment)
        # Plain http only for a site run on this machine
        if(NOT activationUrlFromEnvironment MATCHES "^(https://|http://localhost(:[0-9]+)?/)[A-Za-z0-9./_-]+$" OR activationUrlFromEnvironment MATCHES "/$")
            message(FATAL_ERROR "licensing: ACTIVATION_URL is the website's activation page, as https://example.com/activate, not: ${activationUrlFromEnvironment}")
        endif()

        # Kept, as the keys are
        set(ACTIVATION_URL "${activationUrlFromEnvironment}" CACHE INTERNAL "The activation page last configured with")
    endif()

    if(PLUGIN_REQUIRE_ACTIVATION AND NOT ACTIVATION_URL)
        message(FATAL_ERROR "licensing: PLUGIN_REQUIRE_ACTIVATION needs ACTIVATION_URL, the page its Activate button opens")
    endif()

    string(REGEX REPLACE "[ \t\r\n]" "" keysFromEnvironment "$ENV{LICENCE_KEYS}")

    # Kept so a reconfigure from a shell without the variable trusts the same keys
    if(keysFromEnvironment)
        set(LICENCE_KEYS "${keysFromEnvironment}" CACHE INTERNAL "The licence keys last configured with")
        set(keySource "LICENCE_KEYS")
    else()
        set(keySource "the last configure")
    endif()

    string(REPLACE "," ";" keyList "${LICENCE_KEYS}")

    get_target_property(companyName ${pluginTarget} JUCE_COMPANY_NAME)

    if(NOT companyName OR companyName MATCHES "[^ -~]|[\"\\\\/:*?<>|]")
        message(FATAL_ERROR "licensing: COMPANY_NAME names the salt and the licence's folder, so it must be plain ASCII without any of \" \\ / : * ? < > | : ${companyName}")
    endif()

    get_target_property(productName ${pluginTarget} JUCE_PRODUCT_NAME)

    if(NOT productName OR productName MATCHES "[^ -~]|[\"\\\\/:*?<>|]")
        message(FATAL_ERROR "licensing: PRODUCT_NAME names the salt and the licence file, so it must be plain ASCII without any of \" \\ / : * ? < > | : ${productName}")
    endif()

    set(keyRows "")

    foreach(key IN LISTS keyList)
        if(NOT key MATCHES "^([0-9a-f]+):([0-9a-f]+)$")
            message(FATAL_ERROR "licensing: a key is <keyId>:<publicKey> in lowercase hex, not: ${key}")
        endif()

        set(keyId "${CMAKE_MATCH_1}")
        set(publicKey "${CMAKE_MATCH_2}")
        string(LENGTH "${keyId}" keyIdLength)
        string(LENGTH "${publicKey}" publicKeyLength)

        if(NOT keyIdLength EQUAL 16 OR NOT publicKeyLength EQUAL 64)
            message(FATAL_ERROR "licensing: a key id is 16 hex characters and a public key 64, not: ${key}")
        endif()

        string(APPEND keyRows "        TrustedKey { \"${keyId}\", publicKeyFromHex (\"${publicKey}\").value() },\n")
    endforeach()

    list(LENGTH keyList keyCount)

    if(PLUGIN_REQUIRE_ACTIVATION AND keyCount EQUAL 0)
        message(FATAL_ERROR "licensing: PLUGIN_REQUIRE_ACTIVATION needs LICENCE_KEYS, or no machine could ever be activated")
    endif()

    # A .env kept for something else, or with the keys left empty, is no mistake
    set(keysInDotEnv "")
    if(EXISTS "${CMAKE_SOURCE_DIR}/.env")
        file(STRINGS "${CMAKE_SOURCE_DIR}/.env" keysInDotEnv REGEX "LICENCE_KEYS=[\"']?[0-9a-f]")
    endif()

    if(keyCount GREATER 0)
        message(STATUS "licensing: trusting ${keyCount} key(s) from ${keySource}")
    elseif(keysInDotEnv)
        message(FATAL_ERROR "licensing: there is a .env but LICENCE_KEYS isn't set. Run `source .env`, then configure again")
    else()
        message(STATUS "licensing: LICENCE_KEYS isn't set, so no licence will verify")
    endif()

    set(generatedDir "${CMAKE_CURRENT_BINARY_DIR}/licensing")

    file(CONFIGURE OUTPUT "${generatedDir}/LicenceBuildConfig.h" @ONLY CONTENT
"// Generated by cmake/Licensing.cmake from COMPANY_NAME, PRODUCT_NAME and the LICENCE_KEYS and ACTIVATION_URL environment variables
namespace licensing::config
{
    inline constexpr std::string_view companyName { \"@companyName@\" };
    inline constexpr std::string_view productName { \"@productName@\" };
    inline constexpr std::string_view activationUrl { \"@ACTIVATION_URL@\" };

    inline constexpr std::array<TrustedKey, @keyCount@> trustedKeys {
@keyRows@    };
}
")

    target_include_directories(${pluginTarget} PRIVATE "${generatedDir}")
    target_link_libraries(${pluginTarget} PRIVATE licensing)

    add_library(licensing_fixtures INTERFACE)
    add_library(licensing::fixtures ALIAS licensing_fixtures)
    target_sources(licensing_fixtures INTERFACE "${modulesRepoDir}/Tests/plugin/LicenceBuildTests.cpp")
    target_include_directories(licensing_fixtures INTERFACE "${modulesRepoDir}/Tests")
    target_compile_definitions(licensing_fixtures INTERFACE
        LICENCE_FIXTURES_DIR="${modulesRepoDir}/Tests/fixtures/licence"
        LICENCE_KEYS="${LICENCE_KEYS}"
        LICENCE_PRODUCT_NAME="${productName}")
endfunction()
