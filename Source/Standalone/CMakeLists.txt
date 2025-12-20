cmake_minimum_required(VERSION 3.20)

# Standalone Application configuration
juce_add_gui_app(MIDIXplorerStandalone
    COMPANY_NAME "MIDIXplorer"
    PRODUCT_NAME "MIDI Xplorer"
    BUNDLE_ID "com.midixplorer.standalone"

    NEEDS_CURL TRUE
    NEEDS_WEB_BROWSER FALSE

    DOCUMENT_EXTENSIONS "mid" "midi"
)

# Sources
target_sources(MIDIXplorerStandalone
    PRIVATE
        StandaloneApp.cpp
        PianoSynthesizer.h
        LicenseManager.h
        LicenseManager.cpp
        ActivationDialog.h
        ActivationDialog.cpp
)

# Link libraries
target_link_libraries(MIDIXplorerStandalone
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_cryptography
        juce::juce_data_structures
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Compile definitions
target_compile_definitions(MIDIXplorerStandalone
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=1
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:MIDIXplorerStandalone,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:MIDIXplorerStandalone,JUCE_VERSION>"
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

# Include directories
target_include_directories(MIDIXplorerStandalone
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Copy app after build
if(APPLE)
    add_custom_command(TARGET MIDIXplorerStandalone POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Standalone app built: $<TARGET_BUNDLE_DIR:MIDIXplorerStandalone>"
    )
endif()
