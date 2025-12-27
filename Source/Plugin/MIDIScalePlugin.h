#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Core/MIDIParser/MIDIParser.h"
#include "../Core/ScaleDetector/ScaleDetector.h"
#include "../Core/Database/Database.h"
#include <mutex>
#include <atomic>

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

    // MIDI playback from editor - thread-safe queue
    void addMidiMessage(const juce::MidiMessage& msg);
    void clearMidiQueue();
    double getSampleRate() const { return currentSampleRate; }
    int getBlockSize() const { return currentBlockSize; }

    // Host transport state - updated in processBlock, read from editor
    struct TransportState {
        std::atomic<bool> isPlaying{false};
        std::atomic<double> bpm{120.0};
        std::atomic<double> ppqPosition{0.0};
        std::atomic<double> timeInSeconds{0.0};
    };

    TransportState transportState;

    // MIDI file playback state - persists when editor is closed
    struct PlaybackState {
        std::atomic<bool> isPlaying{false};
        std::atomic<bool> fileLoaded{false};
        std::atomic<double> playbackPosition{0.0};  // 0.0 to 1.0
        std::atomic<double> playbackStartTime{0.0};
        std::atomic<double> playbackStartBeat{0.0};
        std::atomic<int> playbackNoteIndex{0};
        std::atomic<double> fileDuration{0.0};
        std::atomic<double> fileBpm{120.0};
        std::atomic<bool> syncToHost{true};
        std::atomic<float> velocityScale{1.0f};  // 0.0 to 2.0 (0% to 200%)
        std::atomic<int> transposeAmount{0};      // -24 to +24 semitones
        std::atomic<bool> pendingNoteOffs{false}; // Flag to send note-offs in next processBlock
        juce::String currentFilePath;
    };

    PlaybackState playbackState;
    juce::MidiMessageSequence playbackSequence;
    std::mutex sequenceMutex;

    // Active note tracking - maps (channel, note) to note-off time
    // This ensures notes get their proper note-offs even across loops
    struct ActiveNote {
        int channel;
        int originalNoteNumber;
        int noteNumber;
        double noteOffTime;  // When this note should end (in MIDI file time)
    };
    std::vector<ActiveNote> activeNotes;
    std::mutex activeNotesMutex;
    void sendActiveNoteOffs();  // Send note-offs for all active notes (queued)
    void sendActiveNoteOffsImmediate(juce::MidiBuffer& midiMessages);  // Send note-offs directly to buffer
    std::vector<int> getActiveNoteNumbers() {
        std::lock_guard<std::mutex> lock(activeNotesMutex);
        std::vector<int> notes;
        for (const auto& n : activeNotes) notes.push_back(n.noteNumber);
        return notes;
    }

    // Playback control methods
    void setPlaybackPlaying(bool playing) {
        if (!playing && playbackState.isPlaying) {
            // Stop playback - send note offs for all active notes
            sendActiveNoteOffs();
        }
        playbackState.isPlaying = playing;
    }
    bool isPlaybackPlaying() const { return playbackState.isPlaying; }
    void setPlaybackPosition(double pos) { playbackState.playbackPosition = pos; }
    double getPlaybackPosition() const { return playbackState.playbackPosition; }
    void loadPlaybackSequence(const juce::MidiMessageSequence& seq, double duration, double bpm, const juce::String& path, bool preservePosition = false);
    void resetPlayback();
    void seekToPosition(double position);  // Seek playback to a position (0-1)
    void updatePlayback();  // Called from processBlock
    double getFileDuration() const { return playbackState.fileDuration; }
    double getFileBpm() const { return playbackState.fileBpm; }
    void setSyncToHost(bool sync) { playbackState.syncToHost = sync; }
    bool isSyncToHost() const { return playbackState.syncToHost; }
    void setVelocityScale(float scale) { playbackState.velocityScale = scale; }
    float getVelocityScale() const { return playbackState.velocityScale; }
    void setTransposeAmount(int amount) { playbackState.transposeAmount = juce::jlimit(-24, 24, amount); }
    int getTransposeAmount() const { return playbackState.transposeAmount; }
    juce::String getCurrentFilePath() const { return playbackState.currentFilePath; }
    const juce::MidiMessageSequence& getPlaybackSequence() const { return playbackSequence; }
    double getHostBeat() const { return transportState.ppqPosition.load(); }
    bool isHostPlaying() const { return transportState.isPlaying.load(); }

    // Queue MIDI for insertion at DAW playhead
    void queueMidiForInsertion(const juce::MidiFile& midiFile);

private:
    ScaleDetector detector;
    Scale currentScale;

    bool constrainToScale;
    TransformMode transformMode;
    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;

    // MIDI processing
    int constrainNoteToScale(int midiNote);
    std::vector<int> harmonizeNote(int midiNote);
    std::vector<int> arpeggiateNote(int midiNote);

    // MIDI queue for playback from editor
    std::vector<juce::MidiMessage> midiQueue;
    std::mutex midiQueueMutex;

    // MIDI insertion queue
    juce::MidiMessageSequence insertionQueue;
    std::atomic<bool> hasQueuedInsertion{false};
    std::mutex insertionMutex;

    // Parameters
    juce::AudioParameterFloat* scaleConfidence;
    juce::AudioParameterChoice* rootNote;
    juce::AudioParameterChoice* scaleType;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MIDIScalePlugin)
};

} // namespace MIDIScaleDetector
