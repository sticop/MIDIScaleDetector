#include "MIDIScalePlugin.h"

namespace MIDIScaleDetector {

MIDIScalePlugin::MIDIScalePlugin()
    : AudioProcessor(BusesProperties()
                    .withInput("Input", juce::AudioChannelSet::stereo(), true)
                    .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      constrainToScale(false),
      transformMode(TransformMode::Off) {
    
    // Add parameters
    addParameter(scaleConfidence = new juce::AudioParameterFloat(
        "confidence", "Scale Confidence", 0.0f, 1.0f, 0.8f));
    
    addParameter(rootNote = new juce::AudioParameterChoice(
        "root", "Root Note",
        juce::StringArray("C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"),
        0));
    
    addParameter(scaleType = new juce::AudioParameterChoice(
        "scale", "Scale Type",
        juce::StringArray("Major", "Minor", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Locrian"),
        0));
}

MIDIScalePlugin::~MIDIScalePlugin() {}

void MIDIScalePlugin::prepareToPlay(double sampleRate, int samplesPerBlock) {
    // Initialize if needed
}

void MIDIScalePlugin::releaseResources() {
    // Cleanup
}

void MIDIScalePlugin::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    buffer.clear();
    
    if (transformMode == TransformMode::Off) {
        return; // Pass through
    }
    
    juce::MidiBuffer processedMidi;
    
    for (const auto metadata : midiMessages) {
        auto message = metadata.getMessage();
        
        if (message.isNoteOn()) {
            int originalNote = message.getNoteNumber();
            int velocity = message.getVelocity();
            int channel = message.getChannel();
            
            switch (transformMode) {
                case TransformMode::Constrain: {
                    int constrainedNote = constrainNoteToScale(originalNote);
                    processedMidi.addEvent(
                        juce::MidiMessage::noteOn(channel, constrainedNote, (juce::uint8)velocity),
                        metadata.samplePosition);
                    break;
                }
                
                case TransformMode::Harmonize: {
                    auto harmonizedNotes = harmonizeNote(originalNote);
                    for (int note : harmonizedNotes) {
                        processedMidi.addEvent(
                            juce::MidiMessage::noteOn(channel, note, (juce::uint8)velocity),
                            metadata.samplePosition);
                    }
                    break;
                }
                
                case TransformMode::Arpeggiate: {
                    auto arpNotes = arpeggiateNote(originalNote);
                    for (size_t i = 0; i < arpNotes.size(); ++i) {
                        processedMidi.addEvent(
                            juce::MidiMessage::noteOn(channel, arpNotes[i], (juce::uint8)velocity),
                            metadata.samplePosition + i * 100);
                    }
                    break;
                }
                
                default:
                    processedMidi.addEvent(message, metadata.samplePosition);
                    break;
            }
        } else if (message.isNoteOff()) {
            // Handle note off - may need to turn off harmonized notes too
            processedMidi.addEvent(message, metadata.samplePosition);
        } else {
            // Pass through other MIDI messages
            processedMidi.addEvent(message, metadata.samplePosition);
        }
    }
    
    midiMessages.swapWith(processedMidi);
}

juce::AudioProcessorEditor* MIDIScalePlugin::createEditor() {
    return new juce::GenericAudioProcessorEditor(*this);
}

void MIDIScalePlugin::getStateInformation(juce::MemoryBlock& destData) {
    juce::MemoryOutputStream stream(destData, true);
    
    stream.writeInt(static_cast<int>(currentScale.root));
    stream.writeInt(static_cast<int>(currentScale.type));
    stream.writeFloat(static_cast<float>(currentScale.confidence));
    stream.writeBool(constrainToScale);
    stream.writeInt(static_cast<int>(transformMode));
}

void MIDIScalePlugin::setStateInformation(const void* data, int sizeInBytes) {
    juce::MemoryInputStream stream(data, static_cast<size_t>(sizeInBytes), false);
    
    currentScale.root = static_cast<NoteName>(stream.readInt());
    currentScale.type = static_cast<ScaleType>(stream.readInt());
    currentScale.confidence = stream.readFloat();
    constrainToScale = stream.readBool();
    transformMode = static_cast<TransformMode>(stream.readInt());
}

void MIDIScalePlugin::loadMIDIFile(const juce::File& file) {
    MIDIFile midiFile;
    MIDIParser parser;
    
    if (parser.parse(file.getFullPathName().toStdString(), midiFile)) {
        HarmonicAnalysis analysis = detector.analyze(midiFile);
        currentScale = analysis.primaryScale;
        
        // Update parameters
        rootNote->setValueNotifyingHost(static_cast<float>(currentScale.root) / 11.0f);
        scaleConfidence->setValueNotifyingHost(static_cast<float>(currentScale.confidence));
    }
}

int MIDIScalePlugin::constrainNoteToScale(int midiNote) {
    if (currentScale.intervals.empty()) {
        return midiNote;
    }
    
    int octave = midiNote / 12;
    int pitchClass = midiNote % 12;
    int rootPitch = static_cast<int>(currentScale.root);
    
    // Find closest note in scale
    int minDist = 12;
    int closestNote = midiNote;
    
    for (int interval : currentScale.intervals) {
        int scaleNote = (rootPitch + interval) % 12;
        int dist = std::abs(pitchClass - scaleNote);
        
        if (dist < minDist) {
            minDist = dist;
            closestNote = octave * 12 + scaleNote;
        }
    }
    
    return closestNote;
}

std::vector<int> MIDIScalePlugin::harmonizeNote(int midiNote) {
    std::vector<int> notes;
    notes.push_back(midiNote);
    
    if (currentScale.intervals.size() >= 3) {
        // Add third
        int third = constrainNoteToScale(midiNote + 4);
        if (third != midiNote) {
            notes.push_back(third);
        }
        
        // Add fifth
        int fifth = constrainNoteToScale(midiNote + 7);
        if (fifth != midiNote && fifth != third) {
            notes.push_back(fifth);
        }
    }
    
    return notes;
}

std::vector<int> MIDIScalePlugin::arpeggiateNote(int midiNote) {
    std::vector<int> notes;
    
    if (currentScale.intervals.empty()) {
        notes.push_back(midiNote);
        return notes;
    }
    
    int octave = midiNote / 12;
    int rootPitch = static_cast<int>(currentScale.root);
    
    // Create arpeggio from scale intervals
    for (size_t i = 0; i < std::min(size_t(4), currentScale.intervals.size()); ++i) {
        int note = octave * 12 + (rootPitch + currentScale.intervals[i]) % 12;
        notes.push_back(note);
    }
    
    return notes;
}

} // namespace MIDIScaleDetector

// JUCE plugin creation
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new MIDIScaleDetector::MIDIScalePlugin();
}
