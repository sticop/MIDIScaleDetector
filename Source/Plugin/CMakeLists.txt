cmake_minimum_required(VERSION 3.20)

# Add binary resources for Plugin (logo image)
juce_add_binary_data(MIDIXplorerPluginAssets
    HEADER_NAME "BinaryData.h"
    NAMESPACE BinaryData
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/../../Resources/logomidi.png
)

# Plugin configuration
juce_add_plugin(MIDIXplorerPlugin
    COMPANY_NAME "MIDIXplorer"
    PLUGIN_MANUFACTURER_CODE Msdt
    PLUGIN_CODE Msdp
    FORMATS VST3 AU
    PRODUCT_NAME "MIDI Xplorer"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/../../Resources/logomidi.png"

    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE

    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_COPY_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/VST3"
    AU_COPY_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/Components"
)

# Plugin sources
target_sources(MIDIXplorerPlugin
    PRIVATE
        MIDIScalePlugin.cpp
        MIDIScalePlugin.h
        PluginEditor.cpp
        PluginEditor.h
        ${CMAKE_CURRENT_SOURCE_DIR}/../Standalone/LicenseManager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Standalone/LicenseManager.h
        ${CMAKE_CURRENT_SOURCE_DIR}/../Standalone/ActivationDialog.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Standalone/ActivationDialog.h
)

# Link libraries
target_link_libraries(MIDIXplorerPlugin
    PRIVATE
        MIDIXplorerCore
        MIDIXplorerPluginAssets
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_cryptography
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Compile definitions
target_compile_definitions(MIDIXplorerPlugin
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)
