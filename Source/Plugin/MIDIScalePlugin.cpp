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

void MIDIScalePlugin::prepareToPlay(double sampleRate, int samplesPerBlock) {
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;
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

    // Check if we need to send note-offs (from pause/stop/seek)
    if (playbackState.pendingNoteOffs.exchange(false)) {
        sendActiveNoteOffsImmediate(midiMessages);
    }

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
                // Send note-offs immediately to this buffer (not queued)
                sendActiveNoteOffsImmediate(midiMessages);
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

void MIDIScalePlugin::loadPlaybackSequence(const juce::MidiMessageSequence& seq, double duration, double bpm, const juce::String& path, bool preservePosition) {
    std::lock_guard<std::mutex> lock(sequenceMutex);

    // Copy the sequence as-is - no truncation
    playbackSequence = seq;
    playbackSequence.sort();
    playbackSequence.updateMatchedPairs();

    // Find the first note-on event time
    double firstNoteTime = 0.0;
    for (int i = 0; i < playbackSequence.getNumEvents(); i++) {
        auto* event = playbackSequence.getEventPointer(i);
        if (event->message.isNoteOn()) {
            firstNoteTime = event->message.getTimeStamp();
            break;
        }
    }
    playbackState.firstNoteTime.store(firstNoteTime);

    playbackState.fileDuration.store(duration);
    playbackState.fileBpm.store(bpm);
    playbackState.currentFilePath = path;
    playbackState.fileLoaded.store(true);
    playbackState.playbackNoteIndex.store(0);
    playbackState.playbackPosition.store(0.0);

    if (!preservePosition) {
        // If host is already playing when file is loaded, start playback immediately
        bool hostIsPlaying = transportState.isPlaying.load();
        bool shouldSync = playbackState.syncToHost.load();
        if (hostIsPlaying && shouldSync) {
            playbackState.isPlaying.store(true);
            resetPlayback();
        }
    }
}

void MIDIScalePlugin::resetPlayback() {
    // Get the first note time to start playback from there
    double firstNoteTime = playbackState.firstNoteTime.load();
    double bpm = playbackState.fileBpm.load();
    if (bpm <= 0) bpm = 120.0;
    double duration = playbackState.fileDuration.load();
    
    // Calculate the position as a fraction (0-1) for the first note
    double firstNotePosition = (duration > 0) ? (firstNoteTime / duration) : 0.0;
    
    // Find the note index for the first note
    int firstNoteIndex = 0;
    {
        std::lock_guard<std::mutex> lock(sequenceMutex);
        for (int i = 0; i < playbackSequence.getNumEvents(); i++) {
            auto* event = playbackSequence.getEventPointer(i);
            if (event->message.getTimeStamp() >= firstNoteTime) {
                firstNoteIndex = i;
                break;
            }
        }
    }
    
    playbackState.playbackNoteIndex.store(firstNoteIndex);
    playbackState.playbackPosition.store(firstNotePosition);
    
    // Calculate beats offset for the first note position
    double beatsOffset = (firstNoteTime * bpm) / 60.0;
    
    // Adjust start time/beat to account for starting at first note
    playbackState.playbackStartTime.store(juce::Time::getMillisecondCounterHiRes() / 1000.0 - firstNoteTime);

    // When syncing to host, quantize start to current beat position
    // This ensures the MIDI file starts at beat 0 = host current beat
    double currentBeat = transportState.ppqPosition.load();
    playbackState.playbackStartBeat.store(currentBeat - beatsOffset);
}

void MIDIScalePlugin::seekToPosition(double position) {
    // Send note-offs for any currently playing notes
    sendActiveNoteOffs();

    // Calculate the new time position
    double duration = playbackState.fileDuration.load();
    double newTime = position * duration;
    double bpm = playbackState.fileBpm.load();
    if (bpm <= 0) bpm = 120.0;

    // Calculate beats offset for the new position in the MIDI file
    double beatsOffset = (newTime * bpm) / 60.0;

    // Get current host position
    double currentBeat = transportState.ppqPosition.load();

    // Immediate seek - set start beat so that at current host position, we're at newTime
    // This allows the playhead to move immediately while staying in sync with host
    playbackState.playbackStartBeat.store(currentBeat - beatsOffset);
    playbackState.playbackStartTime.store(juce::Time::getMillisecondCounterHiRes() / 1000.0 - newTime);

    // Update position for UI
    playbackState.playbackPosition.store(position);

    // Skip notes before the new position
    std::lock_guard<std::mutex> lock(sequenceMutex);
    int noteIndex = 0;
    while (noteIndex < playbackSequence.getNumEvents()) {
        auto* event = playbackSequence.getEventPointer(noteIndex);
        if (event->message.getTimeStamp() < newTime) {
            noteIndex++;
        } else {
            break;
        }
    }
    playbackState.playbackNoteIndex.store(noteIndex);
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
            bool atStart = playbackState.playbackNoteIndex.load() == 0 &&
                           playbackState.playbackPosition.load() <= 0.0;
            if (!atStart) {
                sendActiveNoteOffs();
                playbackState.playbackNoteIndex.store(0);
            }
            playbackState.playbackStartBeat.store(hostBeat);
            currentTime = 0;
        }
    } else {
        currentTime = juce::Time::getMillisecondCounterHiRes() / 1000.0 - playbackState.playbackStartTime.load();
        if (currentTime < 0) currentTime = 0;
    }

    // Check if we've reached the end - do a clean reset
    if (currentTime >= totalDuration) {
        // Send note-offs for any still-active notes BEFORE resetting
        // This prevents notes that span the loop point from getting stuck
        {
            std::lock_guard<std::mutex> noteLock(activeNotesMutex);
            for (const auto& an : activeNotes) {
                addMidiMessage(juce::MidiMessage::noteOff(an.channel, an.noteNumber));
            }
            activeNotes.clear();
        }

        // Reset note index to start from scratch
        playbackState.playbackNoteIndex.store(0);

        // Reset timing to start a fresh loop - ensure currentTime becomes exactly 0
        if (synced) {
            // Align playback start to current host beat so currentTime = 0
            playbackState.playbackStartBeat.store(hostBeat);
        } else {
            // Reset playback start time to now so currentTime = 0
            playbackState.playbackStartTime.store(juce::Time::getMillisecondCounterHiRes() / 1000.0);
        }

        // Don't play notes in this cycle - let the next processBlock handle it
        // This prevents notes at time 0 from playing immediately after reset
        playbackState.playbackPosition.store(0.0);
        return;
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
                int originalNote = msg.getNoteNumber();
                // Apply transpose
                int transposedNote = juce::jlimit(0, 127, originalNote + playbackState.transposeAmount.load());
                // Apply velocity scaling
                int velocity = msg.getVelocity();
                velocity = juce::jlimit(1, 127, (int)(velocity * velScale));
                msg = juce::MidiMessage::noteOn(msg.getChannel(), transposedNote, (juce::uint8)velocity);
                msg.setTimeStamp(eventTime);
                addMidiMessage(msg);

                // Track this note so we can send proper note-off on stop/seek/loop
                std::lock_guard<std::mutex> noteLock(activeNotesMutex);
                ActiveNote an;
                an.channel = msg.getChannel();
                an.originalNoteNumber = originalNote;
                an.noteNumber = transposedNote;
                an.noteOffTime = event->noteOffObject != nullptr
                    ? event->noteOffObject->message.getTimeStamp()
                    : eventTime;
                activeNotes.push_back(an);
            } else if (msg.isNoteOff()) {
                int originalNote = msg.getNoteNumber();
                int transposedNoteOff = juce::jlimit(0, 127, originalNote + playbackState.transposeAmount.load());

                // Remove the matching active note (use original note to find correct transposed note)
                {
                    std::lock_guard<std::mutex> noteLock(activeNotesMutex);
                    for (auto it = activeNotes.begin(); it != activeNotes.end(); ++it) {
                        if (it->channel == msg.getChannel() && it->originalNoteNumber == originalNote) {
                            transposedNoteOff = it->noteNumber;
                            activeNotes.erase(it);
                            break;
                        }
                    }
                }

                msg = juce::MidiMessage::noteOff(msg.getChannel(), transposedNoteOff);
                addMidiMessage(msg);
            }
            noteIndex++;
        } else {
            break;
        }
    }
    playbackState.playbackNoteIndex.store(noteIndex);
}

void MIDIScalePlugin::sendActiveNoteOffs() {
    // Send note-offs through the queue AND set the flag for processBlock
    // This dual approach ensures note-offs are sent whether processBlock runs immediately or later

    // First, queue note-offs for all active notes
    {
        std::lock_guard<std::mutex> noteLock(activeNotesMutex);
        for (const auto& an : activeNotes) {
            addMidiMessage(juce::MidiMessage::noteOff(an.channel, an.noteNumber));
        }
        activeNotes.clear();
    }

    // Also queue CC#123 (All Notes Off) on all channels as safety fallback
    for (int ch = 1; ch <= 16; ++ch) {
        addMidiMessage(juce::MidiMessage::controllerEvent(ch, 123, 0));
    }

    // Set the flag as well for immediate processing in processBlock
    playbackState.pendingNoteOffs.store(true);
}

void MIDIScalePlugin::sendActiveNoteOffsImmediate(juce::MidiBuffer& midiMessages) {
    std::lock_guard<std::mutex> noteLock(activeNotesMutex);

    // Send individual note-offs for each active note directly to buffer
    for (const auto& an : activeNotes) {
        midiMessages.addEvent(juce::MidiMessage::noteOff(an.channel, an.noteNumber), 0);
    }
    activeNotes.clear();

    // Also send CC#123 (All Notes Off) on all channels as a safety fallback
    // This matches the standalone app behavior and ensures no stuck notes
    for (int ch = 1; ch <= 16; ++ch) {
        midiMessages.addEvent(juce::MidiMessage::controllerEvent(ch, 123, 0), 0);
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
