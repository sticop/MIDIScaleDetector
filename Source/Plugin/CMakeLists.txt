cmake_minimum_required(VERSION 3.20)

# Plugin configuration
juce_add_plugin(MIDIScaleDetectorPlugin
    COMPANY_NAME "MIDIScaleDetector"
    PLUGIN_MANUFACTURER_CODE Msdt
    PLUGIN_CODE Msdp
    FORMATS VST3 AU
    PRODUCT_NAME "MIDI Scale Detector"

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
target_sources(MIDIScaleDetectorPlugin
    PRIVATE
        MIDIScalePlugin.cpp
        MIDIScalePlugin.h
)

# Link libraries
target_link_libraries(MIDIScaleDetectorPlugin
    PRIVATE
        MIDIScaleDetectorCore
        juce::juce_audio_utils
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Compile definitions
target_compile_definitions(MIDIScaleDetectorPlugin
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)
