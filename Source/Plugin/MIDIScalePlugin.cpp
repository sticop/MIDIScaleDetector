#include "MIDIScalePlugin.h"
#include "PluginEditor.h"
#include <set>

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

void MIDIScalePlugin::prepareToPlay(double sampleRate, int /*samplesPerBlock*/) {
    currentSampleRate = sampleRate;
}

void MIDIScalePlugin::releaseResources() {
    clearMidiQueue();
}

void MIDIScalePlugin::addMidiMessage(const juce::MidiMessage& msg) {
    std::lock_guard<std::mutex> lock(midiQueueMutex);
    midiQueue.push_back(msg);
}

void MIDIScalePlugin::clearMidiQueue() {
    std::lock_guard<std::mutex> lock(midiQueueMutex);
    midiQueue.clear();
}

void MIDIScalePlugin::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    buffer.clear();

    // Update transport state from host playhead - this runs on the audio thread
    bool wasHostPlaying = transportState.isPlaying.load();

    if (auto* playhead = getPlayHead()) {
        if (auto pos = playhead->getPosition()) {
            transportState.isPlaying.store(pos->getIsPlaying(), std::memory_order_relaxed);

            if (auto bpm = pos->getBpm()) {
                transportState.bpm.store(*bpm, std::memory_order_relaxed);
            }

            if (auto ppq = pos->getPpqPosition()) {
                transportState.ppqPosition.store(*ppq, std::memory_order_relaxed);
            }

            if (auto timeInSec = pos->getTimeInSeconds()) {
                transportState.timeInSeconds.store(*timeInSec, std::memory_order_relaxed);
            }

            // Auto-start playback when host starts playing (if sync is enabled and file is loaded)
            bool hostNowPlaying = pos->getIsPlaying();
            if (hostNowPlaying && !wasHostPlaying && playbackState.syncToHost.load() && playbackState.fileLoaded.load()) {
                playbackState.isPlaying.store(true);
                playbackState.playbackNoteIndex.store(0);  // Reset note index
                resetPlayback();
            }
            // Auto-stop when host stops
            else if (!hostNowPlaying && wasHostPlaying && playbackState.syncToHost.load()) {
                playbackState.isPlaying.store(false);  // Stop playback state
                playbackState.playbackNoteIndex.store(0);  // Reset for next play
                // Send all notes off
                for (int ch = 1; ch <= 16; ch++) {
                    addMidiMessage(juce::MidiMessage::allNotesOff(ch));
                }
            }
        }
    }

    // Update MIDI file playback (runs even when editor is closed)
    updatePlayback();

    // First, add any queued MIDI messages from the editor (file playback)
    {
        std::lock_guard<std::mutex> lock(midiQueueMutex);
        for (const auto& msg : midiQueue) {
            midiMessages.addEvent(msg, 0);
        }
        midiQueue.clear();
    }

    if (transformMode == TransformMode::Off) {
        return; // Pass through with any queued messages
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
                            metadata.samplePosition + (int)(i * 100));
                    }
                    break;
                }

                default:
                    processedMidi.addEvent(message, metadata.samplePosition);
                    break;
            }
        } else if (message.isNoteOff()) {
            processedMidi.addEvent(message, metadata.samplePosition);
        } else {
            processedMidi.addEvent(message, metadata.samplePosition);
        }
    }

    midiMessages.swapWith(processedMidi);
}

juce::AudioProcessorEditor* MIDIScalePlugin::createEditor() {
    return new ::MIDIXplorerEditor(*this);
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
        int third = constrainNoteToScale(midiNote + 4);
        if (third != midiNote) {
            notes.push_back(third);
        }

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

    for (size_t i = 0; i < std::min(size_t(4), currentScale.intervals.size()); ++i) {
        int note = octave * 12 + (rootPitch + currentScale.intervals[i]) % 12;
        notes.push_back(note);
    }

    return notes;
}

void MIDIScalePlugin::loadPlaybackSequence(const juce::MidiMessageSequence& seq, double duration, double bpm, const juce::String& path) {
    std::lock_guard<std::mutex> lock(sequenceMutex);

    // Clear and rebuild the sequence, truncating notes at the end boundary
    playbackSequence.clear();

    // Track which notes are on at the end so we can add note-offs
    std::set<std::pair<int, int>> activeNotes;  // channel, note number

    for (int i = 0; i < seq.getNumEvents(); i++) {
        auto* event = seq.getEventPointer(i);
        auto msg = event->message;
        double eventTime = msg.getTimeStamp();

        // Skip events that start after the duration
        if (eventTime >= duration) {
            continue;
        }

        if (msg.isNoteOn()) {
            // Add the note-on
            playbackSequence.addEvent(msg);
            activeNotes.insert({msg.getChannel(), msg.getNoteNumber()});
        } else if (msg.isNoteOff()) {
            // Check if this note-off is within duration
            if (eventTime < duration) {
                playbackSequence.addEvent(msg);
                activeNotes.erase({msg.getChannel(), msg.getNoteNumber()});
            }
            // If note-off is past duration, we'll add one at the end
        } else {
            // Other MIDI events (CC, etc.) - add if within duration
            if (eventTime < duration) {
                playbackSequence.addEvent(msg);
            }
        }
    }

    // Add note-offs at the end boundary for any notes still active
    double endTime = duration - 0.001;  // Slightly before end to ensure clean cutoff
    for (const auto& note : activeNotes) {
        auto noteOff = juce::MidiMessage::noteOff(note.first, note.second);
        noteOff.setTimeStamp(endTime);
        playbackSequence.addEvent(noteOff);
    }

    // Sort by timestamp
    playbackSequence.sort();

    playbackState.fileDuration.store(duration);
    playbackState.fileBpm.store(bpm);
    playbackState.currentFilePath = path;
    playbackState.fileLoaded.store(true);
    playbackState.playbackNoteIndex.store(0);
    playbackState.playbackPosition.store(0.0);
}

void MIDIScalePlugin::resetPlayback() {
    playbackState.playbackNoteIndex.store(0);
    playbackState.playbackPosition.store(0.0);
    playbackState.playbackStartTime.store(juce::Time::getMillisecondCounterHiRes() / 1000.0);
    playbackState.playbackStartBeat.store(transportState.ppqPosition.load());
}

void MIDIScalePlugin::updatePlayback() {
    if (!playbackState.isPlaying.load() || !playbackState.fileLoaded.load()) {
        return;
    }

    std::lock_guard<std::mutex> lock(sequenceMutex);

    if (playbackSequence.getNumEvents() == 0) {
        return;
    }

    double totalDuration = playbackState.fileDuration.load();
    if (totalDuration <= 0) totalDuration = 1.0;

    double midiFileBpm = playbackState.fileBpm.load();
    if (midiFileBpm <= 0) midiFileBpm = 120.0;  // Fallback

    bool synced = playbackState.syncToHost.load() && transportState.isPlaying.load();
    double hostBeat = transportState.ppqPosition.load();

    double currentTime;
    if (synced) {
        double beatsElapsed = hostBeat - playbackState.playbackStartBeat.load();
        currentTime = (beatsElapsed * 60.0) / midiFileBpm;

        // If time is negative (DAW seeked backwards), reset completely
        if (currentTime < 0) {
            // Send all notes off for clean state
            for (int ch = 1; ch <= 16; ch++) {
                addMidiMessage(juce::MidiMessage::allNotesOff(ch));
            }
            playbackState.playbackNoteIndex.store(0);
            playbackState.playbackStartBeat.store(hostBeat);
            currentTime = 0;
        }
    } else {
        currentTime = juce::Time::getMillisecondCounterHiRes() / 1000.0 - playbackState.playbackStartTime.load();
        if (currentTime < 0) currentTime = 0;
    }

    // Check if we've reached the end - do a clean reset
    if (currentTime >= totalDuration) {
        // Send all notes off for a clean slate
        for (int ch = 1; ch <= 16; ch++) {
            addMidiMessage(juce::MidiMessage::allNotesOff(ch));
        }

        // Reset note index to start from scratch
        playbackState.playbackNoteIndex.store(0);

        // Reset timing to start a fresh loop
        if (synced) {
            // Align to the next loop start based on total duration in beats
            double beatsPerLoop = (totalDuration * midiFileBpm) / 60.0;
            double loopsCompleted = std::floor((hostBeat - playbackState.playbackStartBeat.load()) * midiFileBpm / 60.0 / totalDuration);
            playbackState.playbackStartBeat.store(playbackState.playbackStartBeat.load() + loopsCompleted * beatsPerLoop);

            // Recalculate current time
            double beatsElapsed = hostBeat - playbackState.playbackStartBeat.load();
            currentTime = (beatsElapsed * 60.0) / midiFileBpm;
        } else {
            double loopsCompleted = std::floor(currentTime / totalDuration);
            playbackState.playbackStartTime.store(playbackState.playbackStartTime.load() + loopsCompleted * totalDuration);
            currentTime = juce::Time::getMillisecondCounterHiRes() / 1000.0 - playbackState.playbackStartTime.load();
        }

        // Clamp to valid range
        if (currentTime < 0) currentTime = 0;
        if (currentTime >= totalDuration) currentTime = 0;  // Safety
    }

    // Update position for UI
    playbackState.playbackPosition.store(currentTime / totalDuration);

    // Play notes that should have triggered by now
    int noteIndex = playbackState.playbackNoteIndex.load();
    while (noteIndex < playbackSequence.getNumEvents()) {
        auto* event = playbackSequence.getEventPointer(noteIndex);
        double eventTime = event->message.getTimeStamp();

        // Play notes at or before current time
        if (eventTime <= currentTime) {
            auto msg = event->message;
            if (msg.isNoteOn() || msg.isNoteOff()) {
                addMidiMessage(msg);
            }
            noteIndex++;
        } else {
            break;
        }
    }
    playbackState.playbackNoteIndex.store(noteIndex);
}

} // namespace MIDIScaleDetector

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new MIDIScaleDetector::MIDIScalePlugin();
}
