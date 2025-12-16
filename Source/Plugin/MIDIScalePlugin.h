#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Core/MIDIParser/MIDIParser.h"
#include "../Core/ScaleDetector/ScaleDetector.h"
#include "../Core/Database/Database.h"

namespace MIDIScaleDetector {

class MIDIScalePlugin : public juce::AudioProcessor {
public:
    MIDIScalePlugin();
    ~MIDIScalePlugin() override;

    // AudioProcessor interface
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    // Editor
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    // Plugin info
    const juce::String getName() const override { return "MIDI Xplorer"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return true; }

    double getTailLengthSeconds() const override { return 0.0; }

    // Programs
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    // State
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Custom methods
    void loadMIDIFile(const juce::File& file);
    Scale getCurrentScale() const { return currentScale; }
    void setConstrainToScale(bool constrain) { constrainToScale = constrain; }
    bool isConstrainedToScale() const { return constrainToScale; }

    // MIDI transformation modes
    enum class TransformMode {
        Off,           // Pass through
        Constrain,     // Snap to scale
        Harmonize,     // Add harmony notes
        Arpeggiate     // Create arpeggios
    };

    void setTransformMode(TransformMode mode) { transformMode = mode; }
    TransformMode getTransformMode() const { return transformMode; }

private:
    ScaleDetector detector;
    Scale currentScale;

    bool constrainToScale;
    TransformMode transformMode;

    // MIDI processing
    int constrainNoteToScale(int midiNote);
    std::vector<int> harmonizeNote(int midiNote);
    std::vector<int> arpeggiateNote(int midiNote);

    // Parameters
    juce::AudioParameterFloat* scaleConfidence;
    juce::AudioParameterChoice* rootNote;
    juce::AudioParameterChoice* scaleType;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MIDIScalePlugin)
};

} // namespace MIDIScaleDetector
