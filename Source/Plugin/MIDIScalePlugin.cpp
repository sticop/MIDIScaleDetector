#include "MIDIScalePlugin.h"
#include "PluginEditor.h"

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
                // Send note-offs for currently playing notes
                sendActiveNoteOffs();
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

    // Handle MIDI insertion at DAW playhead
    if (hasQueuedInsertion.load()) {
        std::lock_guard<std::mutex> lock(insertionMutex);
        for (int i = 0; i < insertionQueue.getNumEvents(); ++i) {
            auto* event = insertionQueue.getEventPointer(i);
            // Output all MIDI events at sample 0 (will play in sequence based on timestamps)
            int samplePos = static_cast<int>(event->message.getTimeStamp() * currentSampleRate);
            if (samplePos < buffer.getNumSamples()) {
                midiMessages.addEvent(event->message, samplePos);
            }
        }
        insertionQueue.clear();
        hasQueuedInsertion = false;
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

    // Save selected file path
    stream.writeString(playbackState.currentFilePath);
}

void MIDIScalePlugin::setStateInformation(const void* data, int sizeInBytes) {
    juce::MemoryInputStream stream(data, static_cast<size_t>(sizeInBytes), false);

    currentScale.root = static_cast<NoteName>(stream.readInt());
    currentScale.type = static_cast<ScaleType>(stream.readInt());
    currentScale.confidence = stream.readFloat();
    constrainToScale = stream.readBool();
    transformMode = static_cast<TransformMode>(stream.readInt());

    // Load selected file path (if available in saved state)
    if (!stream.isExhausted()) {
        playbackState.currentFilePath = stream.readString();
    }
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

    // Copy the sequence as-is - no truncation
    playbackSequence = seq;
    playbackSequence.sort();

    playbackState.fileDuration.store(duration);
    playbackState.fileBpm.store(bpm);
    playbackState.currentFilePath = path;
    playbackState.fileLoaded.store(true);
    playbackState.playbackNoteIndex.store(0);
    playbackState.playbackPosition.store(0.0);

    // If host is already playing when file is loaded, start playback immediately
    bool hostIsPlaying = transportState.isPlaying.load();
    bool shouldSync = playbackState.syncToHost.load();
    if (hostIsPlaying && shouldSync) {
        playbackState.isPlaying.store(true);
        resetPlayback();
    }
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
            // Send note-offs for currently playing notes
            sendActiveNoteOffs();
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
        // Send note-offs for currently active notes
        sendActiveNoteOffs();

        // Reset note index to start from scratch
        playbackState.playbackNoteIndex.store(0);

        // Reset timing to start a fresh loop
        if (synced) {
            // Align to the next loop start based on total duration in beats
            double beatsPerLoop = (totalDuration * midiFileBpm) / 60.0;
            double beatsElapsedTotal = hostBeat - playbackState.playbackStartBeat.load();
            double loopsCompleted = std::floor(beatsElapsedTotal / beatsPerLoop);
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
    // Get velocity scale
    float velScale = playbackState.velocityScale.load();

    while (noteIndex < playbackSequence.getNumEvents()) {
        auto* event = playbackSequence.getEventPointer(noteIndex);
        double eventTime = event->message.getTimeStamp();

        // Play notes at or before current time
        if (eventTime <= currentTime) {
            auto msg = event->message;
            if (msg.isNoteOn()) {
                // Apply transpose
                int transposedNote = juce::jlimit(0, 127, msg.getNoteNumber() + playbackState.transposeAmount.load());
                // Apply velocity scaling
                int velocity = msg.getVelocity();
                velocity = juce::jlimit(1, 127, (int)(velocity * velScale));
                msg = juce::MidiMessage::noteOn(msg.getChannel(), transposedNote, (juce::uint8)velocity);
                msg.setTimeStamp(eventTime);
                addMidiMessage(msg);

                // Track this note so we can send proper note-off
                if (event->noteOffObject != nullptr) {
                    std::lock_guard<std::mutex> noteLock(activeNotesMutex);
                    ActiveNote an;
                    an.channel = msg.getChannel();
                    an.noteNumber = transposedNote;
                    an.noteOffTime = event->noteOffObject->message.getTimeStamp();
                    activeNotes.push_back(an);
                }
            } else if (msg.isNoteOff()) {
                // Apply transpose to note-off too
                int transposedNoteOff = juce::jlimit(0, 127, msg.getNoteNumber() + playbackState.transposeAmount.load());
                msg = juce::MidiMessage::noteOff(msg.getChannel(), transposedNoteOff);
                addMidiMessage(msg);

                // Remove from active notes
                {
                    std::lock_guard<std::mutex> noteLock(activeNotesMutex);
                    activeNotes.erase(
                        std::remove_if(activeNotes.begin(), activeNotes.end(),
                            [&](const ActiveNote& an) {
                                return an.channel == msg.getChannel() && an.noteNumber == transposedNoteOff;
                            }),
                        activeNotes.end());
                }
            }
            noteIndex++;
        } else {
            break;
        }
    }
    playbackState.playbackNoteIndex.store(noteIndex);
}

void MIDIScalePlugin::sendActiveNoteOffs() {
    std::lock_guard<std::mutex> noteLock(activeNotesMutex);
    
    // Send individual note-offs for each active note
    for (const auto& an : activeNotes) {
        addMidiMessage(juce::MidiMessage::noteOff(an.channel, an.noteNumber));
    }
    activeNotes.clear();
    
    // Also send All-Notes-Off (CC 123) and All-Sound-Off (CC 120) on all channels for reliability
    for (int channel = 1; channel <= 16; ++channel) {
        addMidiMessage(juce::MidiMessage::allNotesOff(channel));
        addMidiMessage(juce::MidiMessage::allSoundOff(channel));
    }
}



void MIDIScalePlugin::queueMidiForInsertion(const juce::MidiFile& midiFile) {
    std::lock_guard<std::mutex> lock(insertionMutex);

    insertionQueue.clear();

    // Get all tracks and merge them
    for (int track = 0; track < midiFile.getNumTracks(); ++track) {
        const auto* trackSeq = midiFile.getTrack(track);
        if (trackSeq) {
            for (int i = 0; i < trackSeq->getNumEvents(); ++i) {
                auto* event = trackSeq->getEventPointer(i);
                if (event->message.isNoteOnOrOff()) {
                    insertionQueue.addEvent(event->message);
                }
            }
        }
    }

    // Sort by timestamp
    insertionQueue.sort();
    insertionQueue.updateMatchedPairs();

    hasQueuedInsertion = true;
}

} // namespace MIDIScaleDetector

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new MIDIScaleDetector::MIDIScalePlugin();
}
