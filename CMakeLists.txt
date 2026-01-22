cmake_minimum_required(VERSION 3.22)
project(BeccaToneAmp VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE - use JUCE_PATH if provided (CI), otherwise fetch from GitHub
if(DEFINED JUCE_PATH AND EXISTS "${JUCE_PATH}/CMakeLists.txt")
    message(STATUS "Using JUCE from: ${JUCE_PATH}")
    add_subdirectory("${JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
else()
    message(STATUS "Fetching JUCE from GitHub...")
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
    )
    FetchContent_MakeAvailable(JUCE)
endif()

# BeatConnect Plugin SDK (activation, asset downloads)
add_subdirectory(beatconnect-sdk/sdk/activation)
# SDK needs cryptography module for SHA256
target_link_libraries(beatconnect_activation PRIVATE juce::juce_cryptography)

# Determine plugin formats - AU only on macOS
set(PLUGIN_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND PLUGIN_FORMATS AU)
endif()

# Plugin target with WebView2 support
juce_add_plugin(BeccaToneAmp
    COMPANY_NAME "BeatConnect"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Beat
    PLUGIN_CODE Bcta
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "Becca Tone Amp"
    NEEDS_WEBVIEW2 TRUE
)

# Source files
target_sources(BeccaToneAmp
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

target_compile_definitions(BeccaToneAmp
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_WIN_WEBVIEW2=1
        # Static linking embeds the WebView2 loader into the binary - no DLL needed at runtime!
        JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        # Development mode - set to 0 for production builds
        BECCA_TONE_AMP_DEV_MODE=0
)

target_link_libraries(BeccaToneAmp
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
        beatconnect_activation
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Copy WebUI resources after build (for production)
# Copy to Standalone
add_custom_command(TARGET BeccaToneAmp_Standalone POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/Resources/WebUI"
        "$<TARGET_FILE_DIR:BeccaToneAmp_Standalone>/Resources/WebUI"
    COMMENT "Copying WebUI resources to Standalone..."
)
# Copy to VST3 build output
add_custom_command(TARGET BeccaToneAmp_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/Resources/WebUI"
        "$<TARGET_FILE_DIR:BeccaToneAmp_VST3>/../Resources/WebUI"
    COMMENT "Copying WebUI resources to VST3..."
)
# Copy to AU build output (macOS only)
if(APPLE)
    add_custom_command(TARGET BeccaToneAmp_AU POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/Resources/WebUI"
            "$<TARGET_FILE_DIR:BeccaToneAmp_AU>/../Resources/WebUI"
        COMMENT "Copying WebUI resources to AU..."
    )
endif()

# Also copy WebUI to installed VST3 location (Windows)
if(WIN32)
    add_custom_command(TARGET BeccaToneAmp_VST3 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/Resources/WebUI"
            "$ENV{CommonProgramFiles}/VST3/Becca Tone Amp.vst3/Contents/Resources/WebUI"
        COMMENT "Copying WebUI resources to installed VST3..."
    )
endif()

# ==============================================================================
# BeatConnect SDK Integration
# ==============================================================================

# Option to enable BeatConnect activation (set by CI or manually)
option(BEATCONNECT_ENABLE_ACTIVATION "Enable BeatConnect activation key support" OFF)

# Embed project_data.json if it exists (injected at build time by BeatConnect CI)
# VERBOSE: Show exactly what path we're checking
message(STATUS "=== BeatConnect SDK Configuration ===")
message(STATUS "CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")
message(STATUS "Looking for project_data.json at: ${CMAKE_SOURCE_DIR}/resources/project_data.json")
message(STATUS "BEATCONNECT_ENABLE_ACTIVATION option: ${BEATCONNECT_ENABLE_ACTIVATION}")

if(EXISTS "${CMAKE_SOURCE_DIR}/resources/project_data.json")
    message(STATUS "Found project_data.json - embedding as BinaryData")
    # Show file info for debugging
    file(SIZE "${CMAKE_SOURCE_DIR}/resources/project_data.json" PROJECT_DATA_SIZE)
    message(STATUS "  File size: ${PROJECT_DATA_SIZE} bytes")
    file(READ "${CMAKE_SOURCE_DIR}/resources/project_data.json" PROJECT_DATA_CONTENT)
    string(LENGTH "${PROJECT_DATA_CONTENT}" PROJECT_DATA_LEN)
    message(STATUS "  Content length: ${PROJECT_DATA_LEN} characters")
    # Show first 200 chars to verify content without leaking sensitive data
    string(SUBSTRING "${PROJECT_DATA_CONTENT}" 0 200 PROJECT_DATA_PREVIEW)
    message(STATUS "  Content preview: ${PROJECT_DATA_PREVIEW}...")

    juce_add_binary_data(BeccaToneAmp_ProjectData
        HEADER_NAME "ProjectData.h"
        NAMESPACE ProjectData
        SOURCES
            resources/project_data.json
    )
    target_link_libraries(BeccaToneAmp PRIVATE BeccaToneAmp_ProjectData)
    target_compile_definitions(BeccaToneAmp PUBLIC HAS_PROJECT_DATA=1)
    message(STATUS "  HAS_PROJECT_DATA = 1")
else()
    message(STATUS "No project_data.json found - BeatConnect features will use SDK defaults")
    message(STATUS "  Listing contents of ${CMAKE_SOURCE_DIR}/resources/ (if it exists):")
    if(EXISTS "${CMAKE_SOURCE_DIR}/resources")
        file(GLOB RESOURCES_FILES "${CMAKE_SOURCE_DIR}/resources/*")
        foreach(FILE ${RESOURCES_FILES})
            message(STATUS "    - ${FILE}")
        endforeach()
    else()
        message(STATUS "    Directory does not exist!")
    endif()
    target_compile_definitions(BeccaToneAmp PUBLIC HAS_PROJECT_DATA=0)
    message(STATUS "  HAS_PROJECT_DATA = 0")
endif()

# BeatConnect Activation SDK integration (already linked above, just need compile def)
if(BEATCONNECT_ENABLE_ACTIVATION)
    target_compile_definitions(BeccaToneAmp PUBLIC BEATCONNECT_ACTIVATION_ENABLED=1)
    message(STATUS "  BEATCONNECT_ACTIVATION_ENABLED = 1 (activation will be compiled in)")
else()
    target_compile_definitions(BeccaToneAmp PUBLIC BEATCONNECT_ACTIVATION_ENABLED=0)
    message(STATUS "  BEATCONNECT_ACTIVATION_ENABLED = 0 (activation disabled)")
endif()

message(STATUS "=== End BeatConnect SDK Configuration ===")
