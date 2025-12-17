#include "PluginEditor.h"
#include "MIDIScalePlugin.h"

namespace {
    int getKeyOrder(const juce::String& key) {
        if (key == "---") return 9999;
        const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        for (int i = 0; i < 12; i++) {
            if (key.startsWith(noteNames[i])) {
                int order = i * 2;
                if (key.contains("Minor")) order += 1;
                return order;
            }
        }
        return 9999;
    }
}

MIDIXplorerEditor::MIDIXplorerEditor(juce::AudioProcessor& p)
    : AudioProcessorEditor(&p) {

    pluginProcessor = dynamic_cast<MIDIScaleDetector::MIDIScalePlugin*>(&p);
    setWantsKeyboardFocus(true);

    // Setup library sidebar
    librariesLabel.setText("Libraries", juce::dontSendNotification);
    librariesLabel.setFont(juce::FontOptions(14.0f).withStyle("Bold"));
    librariesLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(librariesLabel);

    addLibraryBtn.onClick = [this]() { addLibrary(); };
    addAndMakeVisible(addLibraryBtn);

    libraryModel = std::make_unique<LibraryListModel>(*this);
    libraryListBox.setModel(libraryModel.get());
    libraryListBox.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff2a2a2a));
    libraryListBox.setRowHeight(28);
    addAndMakeVisible(libraryListBox);

    // File list header
    fileCountLabel.setText("0 files", juce::dontSendNotification);
    fileCountLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(fileCountLabel);

    // Key filter dropdown
    keyFilterCombo.addItem("All Keys", 1);
    keyFilterCombo.setSelectedId(1);
    keyFilterCombo.onChange = [this]() { filterFiles(); };
    addAndMakeVisible(keyFilterCombo);

    // Sort dropdown
    sortCombo.addItem("Sort: Scale", 1);
    sortCombo.addItem("Sort: Duration", 2);
    sortCombo.addItem("Sort: Name", 3);
    sortCombo.setSelectedId(1);
    sortCombo.onChange = [this]() { sortFiles(); filterFiles(); };
    addAndMakeVisible(sortCombo);

    // Search box with modern rounded style
    searchBox.setTextToShowWhenEmpty("🔍 Search MIDI files...", juce::Colours::grey);
    searchBox.onTextChange = [this]() { filterFiles(); };
    searchBox.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff2a2a2a));
    searchBox.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff444444));
    searchBox.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xff0078d4));
    searchBox.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    searchBox.setFont(juce::Font(14.0f));
    searchBox.setJustification(juce::Justification::centredLeft);
    addAndMakeVisible(searchBox);

    // File list with custom draggable listbox
    fileModel = std::make_unique<FileListModel>(*this);
    fileListBox = std::make_unique<DraggableListBox>("Files", *this);
    fileListBox->setModel(fileModel.get());
    fileListBox->setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff1e1e1e));
    fileListBox->setRowHeight(32);
    fileListBox->setMultipleSelectionEnabled(false);
    addAndMakeVisible(fileListBox.get());

    // Transport controls - Play/Pause toggle button
    playPauseButton.setButtonText(juce::String::fromUTF8("\u25B6"));  // Show play icon initially
    playPauseButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3a3a3a));
    playPauseButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff5a5a5a));  // Hover highlight
    playPauseButton.setColour(juce::TextButton::textColourOffId, juce::Colours::cyan);
    playPauseButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    isPlaying = false;  // Start with playback stopped
    playPauseButton.onClick = [this]() {
        if (!isPlaying) {
            // Start playing
            isPlaying = true;
            playPauseButton.setButtonText(juce::String::fromUTF8("\u23F8"));  // Pause icon
            // If no file is loaded, play the first file
            if (!fileLoaded && !filteredFiles.empty()) {
                selectAndPreview(0);
            } else if (fileLoaded) {
                // Resume playback
                if (playbackNoteIndex == 0) {
                    playbackStartTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
                    playbackStartBeat = getHostBeatPosition();
                }
            }
        } else {
            // Pause
            isPlaying = false;
            playPauseButton.setButtonText(juce::String::fromUTF8("\u25B6"));  // Play icon
            // Send all notes off when pausing
            if (pluginProcessor) {
                for (int ch = 1; ch <= 16; ch++) {
                    pluginProcessor->addMidiMessage(juce::MidiMessage::allNotesOff(ch));
                }
            }
        }
    };
    addAndMakeVisible(playPauseButton);

    syncToHostToggle.setToggleState(true, juce::dontSendNotification);
    syncToHostToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    syncToHostToggle.setColour(juce::ToggleButton::tickColourId, juce::Colours::orange);
    addAndMakeVisible(syncToHostToggle);

    transportSlider.setSliderStyle(juce::Slider::LinearBar);
    transportSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    transportSlider.setRange(0.0, 1.0);
    transportSlider.setVelocityBasedMode(false);
    transportSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xff3a3a3a));
    transportSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff00aaff));
    addAndMakeVisible(transportSlider);


    timeDisplayLabel.setFont(juce::Font(14.0f));
    timeDisplayLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    timeDisplayLabel.setJustificationType(juce::Justification::centredRight);
    timeDisplayLabel.setText("0:00 / 0:00", juce::dontSendNotification);
    addAndMakeVisible(timeDisplayLabel);
    addAndMakeVisible(midiNoteViewer);
    setSize(1000, 700);

    // Load saved libraries
    loadLibraries();
    loadFavorites();

    startTimer(20); // 50 fps for smooth sync
}

MIDIXplorerEditor::~MIDIXplorerEditor() {
    saveLibraries();
    stopTimer();
}

juce::File MIDIXplorerEditor::getSettingsFile() {
    auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    auto pluginDir = appDataDir.getChildFile("MIDIXplorer");
    if (!pluginDir.exists()) {
        pluginDir.createDirectory();
    }
    return pluginDir.getChildFile("libraries.json");
}

void MIDIXplorerEditor::saveLibraries() {
    auto file = getSettingsFile();
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    juce::Array<juce::var> libsArray;

    for (const auto& lib : libraries) {
        juce::DynamicObject::Ptr libObj = new juce::DynamicObject();
        libObj->setProperty("name", lib.name);
        libObj->setProperty("path", lib.path);
        libObj->setProperty("enabled", lib.enabled);
        libsArray.add(juce::var(libObj.get()));
    }
    root->setProperty("libraries", libsArray);

    juce::var jsonVar(root.get());
    auto jsonStr = juce::JSON::toString(jsonVar);
    file.replaceWithText(jsonStr);
}

void MIDIXplorerEditor::loadLibraries() {
    auto file = getSettingsFile();
    if (!file.existsAsFile()) return;

    auto jsonStr = file.loadFileAsString();
    auto jsonVar = juce::JSON::parse(jsonStr);

    if (auto* obj = jsonVar.getDynamicObject()) {
        auto libsVar = obj->getProperty("libraries");
        if (auto* libsArray = libsVar.getArray()) {
            libraries.clear();
            for (const auto& libVar : *libsArray) {
                if (auto* libObj = libVar.getDynamicObject()) {
                    Library lib;
                    lib.name = libObj->getProperty("name").toString();
                    lib.path = libObj->getProperty("path").toString();
                    lib.enabled = libObj->getProperty("enabled");
                    libraries.push_back(lib);
                }
            }
            libraryListBox.updateContent();
            scanLibraries();
        }
    }
}

void MIDIXplorerEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff1a1a1a));

    // Sidebar background
    g.setColour(juce::Colour(0xff252525));
    g.fillRect(0, 0, 200, getHeight());

    // Separator line
    g.setColour(juce::Colour(0xff3a3a3a));
    g.drawVerticalLine(200, 0.0f, (float)getHeight());
}

void MIDIXplorerEditor::resized() {
    auto area = getLocalBounds();

    // Sidebar
    auto sidebar = area.removeFromLeft(200);
    auto sidebarTop = sidebar.removeFromTop(30);
    librariesLabel.setBounds(sidebarTop.removeFromLeft(100).reduced(8, 4));
    addLibraryBtn.setBounds(sidebarTop.reduced(4));
    libraryListBox.setBounds(sidebar.reduced(4));

    // Main content area
    area.removeFromLeft(1); // separator

    // Search bar row (prominent, full width)
    auto searchRow = area.removeFromTop(40);
    searchRow = searchRow.reduced(8, 6);
    searchBox.setBounds(searchRow);
    
    // Controls bar with filters and options
    auto topBar = area.removeFromTop(32);
    topBar = topBar.reduced(8, 2);
    fileCountLabel.setBounds(topBar.removeFromLeft(70));
    keyFilterCombo.setBounds(topBar.removeFromLeft(105).reduced(2));
    sortCombo.setBounds(topBar.removeFromLeft(110).reduced(2));
    topBar.removeFromLeft(8);
    syncToHostToggle.setBounds(topBar.removeFromRight(90));

    // Bottom transport bar
    auto transport = area.removeFromBottom(40).reduced(8, 4);
    playPauseButton.setBounds(transport.removeFromLeft(40));
    transport.removeFromLeft(10);
    timeDisplayLabel.setBounds(transport.removeFromRight(80));
    transportSlider.setBounds(transport);

    // MIDI Note Viewer
    auto noteViewerArea = area.removeFromBottom(120);
    midiNoteViewer.setBounds(noteViewerArea.reduced(4));

    // File list fills the rest
    fileListBox->setBounds(area.reduced(4));
}

void MIDIXplorerEditor::timerCallback() {
    bool synced = syncToHostToggle.getToggleState();
    bool hostPlaying = isHostPlaying();
    double hostBpm = getHostBpm();
    double hostBeat = getHostBeatPosition();

    // Handle host play/stop changes - check this BEFORE early return
    if (synced) {
        if (hostPlaying && !wasHostPlaying) {
            // Host just started playing - start playback immediately
            isPlaying = true;
            playPauseButton.setButtonText(juce::String::fromUTF8("\u23F8"));  // Pause icon

            // If no file is loaded, load the first file
            if (!fileLoaded && !filteredFiles.empty()) {
                selectAndPreview(0);
            }

            playbackNoteIndex = 0;
            playbackStartBeat = hostBeat;
            playbackStartTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;

            // Clear any pending notes
            if (pluginProcessor) {
                pluginProcessor->clearMidiQueue();
            }
        } else if (!hostPlaying && wasHostPlaying) {
            // Host stopped - stop playback and send note-offs
            if (pluginProcessor) {
                // Send all notes off on all channels
                for (int ch = 1; ch <= 16; ch++) {
                    pluginProcessor->addMidiMessage(juce::MidiMessage::allNotesOff(ch));
                }
            }
            playbackNoteIndex = 0;
            // Pause playback - user can resume by clicking a file
            isPlaying = false;
            playPauseButton.setButtonText(juce::String::fromUTF8("\u25B6"));  // Play icon
        }

        // Detect DAW loop: if host beat jumped backwards significantly, DAW looped
        if (hostPlaying && wasHostPlaying && lastBeatPosition > 0) {
            double beatDiff = hostBeat - lastBeatPosition;
            // If beat jumped backwards by more than 1 beat, DAW probably looped
            if (beatDiff < -0.5) {
                // DAW looped back - restart MIDI aligned to exact playhead position
                if (pluginProcessor) {
                    for (int ch = 1; ch <= 16; ch++) {
                        pluginProcessor->addMidiMessage(juce::MidiMessage::allNotesOff(ch));
                    }
                }
                
                // Calculate where in the MIDI file we should be based on host beat fraction
                double beatFraction = hostBeat - std::floor(hostBeat);
                
                // Convert to time offset in MIDI file
                double timeOffsetInFile = (beatFraction * 60.0) / midiFileBpm;
                
                // Find the note index that corresponds to this time
                playbackNoteIndex = 0;
                
                // Set start beat to align with current host position
                playbackStartBeat = hostBeat - (timeOffsetInFile * midiFileBpm / 60.0);
                playbackStartTime = juce::Time::getMillisecondCounterHiRes() / 1000.0 - timeOffsetInFile;
                
                // Immediately play any notes at time 0 or very start
                while (playbackNoteIndex < playbackSequence.getNumEvents()) {
                    auto* event = playbackSequence.getEventPointer(playbackNoteIndex);
                    double eventTime = event->message.getTimeStamp();
                    // Play notes that should have started by now (with small tolerance)
                    if (eventTime <= timeOffsetInFile + 0.01) {
                        auto msg = event->message;
                        if (msg.isNoteOn() || msg.isNoteOff()) {
                            if (pluginProcessor) {
                                pluginProcessor->addMidiMessage(msg);
                            }
                        }
                        playbackNoteIndex++;
                    } else {
                        break;
                    }
                }
            }
        }

        wasHostPlaying = hostPlaying;
    }

    if (!isPlaying || !fileLoaded) {
        return;
    }

    // Handle pending file changes on beat boundary
    if (synced && pendingFileChange && hostPlaying) {
        int currentBeat = (int)std::floor(hostBeat);
        double currentBeatFrac = hostBeat - currentBeat;

        // Check if we crossed a beat boundary
        if (currentBeatFrac < 0.15 || (int)std::floor(lastBeatPosition) != currentBeat) {
            // Apply pending file change
            if (pendingFileIndex >= 0 && pendingFileIndex < (int)filteredFiles.size()) {
                fileListBox->selectRow(pendingFileIndex);
                selectedFileIndex = pendingFileIndex;
                loadSelectedFile();
                playbackStartBeat = std::floor(hostBeat); // Start from this beat
            }
            pendingFileChange = false;
            pendingFileIndex = -1;
        }
    }

    lastBeatPosition = hostBeat;

    if (playbackSequence.getNumEvents() == 0) {
        return;
    }

    double currentTime;
    // When synced, use host BPM; otherwise use MIDI file's original tempo
    double bpm = synced ? hostBpm : midiFileBpm;

    if (synced) {
        // Calculate position based on host beat position relative to when we started
        double beatsElapsed = hostBeat - playbackStartBeat;
        // Convert beats to MIDI file time (seconds at MIDI file's tempo)
        // beatsElapsed beats = beatsElapsed * 60 / midiFileBpm seconds in MIDI file time
        currentTime = (beatsElapsed * 60.0) / midiFileBpm;
    } else {
        // Freerun mode: use real time directly (MIDI file plays at its original tempo)
        currentTime = juce::Time::getMillisecondCounterHiRes() / 1000.0 - playbackStartTime;
    }

    // Use pre-calculated file duration
    double totalDuration = midiFileDuration;
    if (totalDuration <= 0) totalDuration = 1.0;

    // Update transport slider
    double position = std::fmod(currentTime, totalDuration) / totalDuration;
    if (position >= 0 && position <= 1) {
        transportSlider.setValue(position, juce::dontSendNotification);
        midiNoteViewer.setPlaybackPosition(position);
        currentPlaybackPosition = position;
        fileListBox->repaint();  // Refresh to show progress bar

        // Update time display
        double displayTime = std::fmod(currentTime, totalDuration);
        int elapsedMin = (int)(displayTime) / 60;
        int elapsedSec = (int)(displayTime) % 60;
        int totalMin = (int)(totalDuration) / 60;
        int totalSec = (int)(totalDuration) % 60;
        timeDisplayLabel.setText(juce::String::formatted("%d:%02d / %d:%02d", elapsedMin, elapsedSec, totalMin, totalSec), juce::dontSendNotification);
    }

    // Handle looping
    if (currentTime >= totalDuration) {
        // Send all notes off before looping to prevent overlapping notes
        if (pluginProcessor) {
            for (int ch = 1; ch <= 16; ch++) {
                pluginProcessor->addMidiMessage(juce::MidiMessage::allNotesOff(ch));
            }
        }
        // Calculate how much we overshot and preserve it for smooth loop
        double overshoot = std::fmod(currentTime, totalDuration);
        playbackNoteIndex = 0;

        if (synced) {
            // Calculate how many beats the file duration represents
            double fileDurationInBeats = (totalDuration * midiFileBpm) / 60.0;
            // Advance start beat by file duration in beats (preserves fractional timing)
            playbackStartBeat += fileDurationInBeats;
        } else {
            playbackStartTime = juce::Time::getMillisecondCounterHiRes() / 1000.0 - overshoot;
        }
        
        // Immediately play notes at or near time 0 to prevent missing first beat
        while (playbackNoteIndex < playbackSequence.getNumEvents()) {
            auto* event = playbackSequence.getEventPointer(playbackNoteIndex);
            double eventTime = event->message.getTimeStamp();
            if (eventTime <= overshoot + 0.005) {  // Small tolerance for timing
                auto msg = event->message;
                if (msg.isNoteOn() || msg.isNoteOff()) {
                    if (pluginProcessor) {
                        pluginProcessor->addMidiMessage(msg);
                    }
                }
                playbackNoteIndex++;
            } else {
                break;
            }
        }
    }

    // Play notes that should have started by now
    while (playbackNoteIndex < playbackSequence.getNumEvents()) {
        auto* event = playbackSequence.getEventPointer(playbackNoteIndex);
        double eventTime = event->message.getTimeStamp();

        if (eventTime <= currentTime) {
            auto msg = event->message;
            if (msg.isNoteOn() || msg.isNoteOff()) {
                if (pluginProcessor) {
                    pluginProcessor->addMidiMessage(msg);
                }
            }
            playbackNoteIndex++;
        } else {
            break;
        }
    }
}

bool MIDIXplorerEditor::keyPressed(const juce::KeyPress& key) {
    if (filteredFiles.empty()) return false;

    int currentRow = fileListBox->getSelectedRow();

    if (key == juce::KeyPress::upKey) {
        int newRow = std::max(0, currentRow - 1);
        selectAndPreview(newRow);
        return true;
    } else if (key == juce::KeyPress::downKey) {
        int newRow = std::min((int)filteredFiles.size() - 1, currentRow + 1);
        selectAndPreview(newRow);
        return true;
    } else if (key == juce::KeyPress::returnKey) {
        if (currentRow >= 0 && currentRow < (int)filteredFiles.size()) {
            isPlaying = true;
            playPauseButton.setButtonText(juce::String::fromUTF8("\u23F8"));  // Pause icon
            selectAndPreview(currentRow);
            return true;
        }
    }

    return false;
}

void MIDIXplorerEditor::selectAndPreview(int row) {
    if (row < 0 || row >= (int)filteredFiles.size()) return;

    fileListBox->selectRow(row);
    fileListBox->scrollToEnsureRowIsOnscreen(row);
    selectedFileIndex = row;

    // Resume playback when selecting a file
    isPlaying = true;
    playPauseButton.setButtonText(juce::String::fromUTF8("\u23F8"));  // Pause icon

    if (syncToHostToggle.getToggleState() && isHostPlaying()) {
        // Queue the file change for the next beat
        scheduleFileChange();
    } else {
        // Load immediately
        loadSelectedFile();
    }
}

void MIDIXplorerEditor::scheduleFileChange() {
    // Send all notes off immediately when scheduling a file change
    if (auto* pluginProcessor = dynamic_cast<MIDIScaleDetector::MIDIScalePlugin*>(&processor)) {
        for (int ch = 1; ch <= 16; ch++) {
            pluginProcessor->addMidiMessage(juce::MidiMessage::allNotesOff(ch));
        }
    }
    pendingFileChange = true;
    pendingFileIndex = selectedFileIndex;
}

void MIDIXplorerEditor::loadSelectedFile() {
    if (selectedFileIndex < 0 || selectedFileIndex >= (int)filteredFiles.size()) return;

    auto& info = filteredFiles[(size_t)selectedFileIndex];
    juce::File file(info.fullPath);

    if (!file.existsAsFile()) return;

    // Send all notes off to stop any playing notes from previous file
    if (auto* pluginProcessor = dynamic_cast<MIDIScaleDetector::MIDIScalePlugin*>(&processor)) {
        for (int ch = 1; ch <= 16; ch++) {
            pluginProcessor->addMidiMessage(juce::MidiMessage::allNotesOff(ch));
        }
    }

    juce::FileInputStream stream(file);
    if (!stream.openedOk()) return;

    currentMidiFile.readFrom(stream);
    currentMidiFile.convertTimestampTicksToSeconds();

    // Extract tempo from MIDI file
    midiFileBpm = 120.0;
    for (int track = 0; track < currentMidiFile.getNumTracks(); track++) {
        auto* trackSeq = currentMidiFile.getTrack(track);
        if (trackSeq) {
            for (int i = 0; i < trackSeq->getNumEvents(); i++) {
                auto& msg = trackSeq->getEventPointer(i)->message;
                if (msg.isTempoMetaEvent()) {
                    midiFileBpm = 60.0 / msg.getTempoSecondsPerQuarterNote();
                    break;
                }
            }
        }
    }

    // Merge all tracks into one sequence
    playbackSequence.clear();
    for (int track = 0; track < currentMidiFile.getNumTracks(); track++) {
        auto* trackSeq = currentMidiFile.getTrack(track);
        if (trackSeq) {
            for (int i = 0; i < trackSeq->getNumEvents(); i++) {
                auto* event = trackSeq->getEventPointer(i);
                playbackSequence.addEvent(event->message);
            }
        }
    }
    playbackSequence.sort();
    playbackSequence.updateMatchedPairs();

    // Calculate actual file duration (find the last note-off time)
    midiFileDuration = 0.0;
    for (int i = 0; i < playbackSequence.getNumEvents(); i++) {
        auto* event = playbackSequence.getEventPointer(i);
        double eventTime = event->message.getTimeStamp();
        if (eventTime > midiFileDuration) {
            midiFileDuration = eventTime;
        }
        // For note-on events, check if there is a matched note-off
        if (event->message.isNoteOn() && event->noteOffObject != nullptr) {
            double noteOffTime = event->noteOffObject->message.getTimeStamp();
            if (noteOffTime > midiFileDuration) {
                midiFileDuration = noteOffTime;
            }
        }
    }
    if (midiFileDuration <= 0) midiFileDuration = 1.0;

    // Update MIDI note viewer
    midiNoteViewer.setSequence(&playbackSequence, midiFileDuration);

    // Reset playback position
    playbackNoteIndex = 0;
    fileLoaded = true;

    // Always set both timing references for proper sync
    playbackStartBeat = getHostBeatPosition();
    playbackStartTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;

    if (pluginProcessor) {
        pluginProcessor->clearMidiQueue();
    }
}

void MIDIXplorerEditor::stopPlayback() {
    if (pluginProcessor) {
        pluginProcessor->clearMidiQueue();
    }
    playbackNoteIndex = 0;
}

void MIDIXplorerEditor::restartPlayback() {
    if (syncToHostToggle.getToggleState() && isHostPlaying()) {
        // Queue restart for the next beat to maintain sync
        scheduleFileChange();
    } else {
        // Restart immediately in freerun mode
        if (pluginProcessor) {
            for (int ch = 1; ch <= 16; ch++) {
                pluginProcessor->addMidiMessage(juce::MidiMessage::allNotesOff(ch));
            }
        }
        playbackNoteIndex = 0;
        playbackStartTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        playbackStartBeat = getHostBeatPosition();
    }
}

double MIDIXplorerEditor::getHostBpm() {
    if (pluginProcessor) {
        return pluginProcessor->transportState.bpm.load(std::memory_order_relaxed);
    }
    return lastHostBpm;
}

double MIDIXplorerEditor::getHostBeatPosition() {
    if (pluginProcessor) {
        return pluginProcessor->transportState.ppqPosition.load(std::memory_order_relaxed);
    }
    return 0.0;
}

bool MIDIXplorerEditor::isHostPlaying() {
    if (pluginProcessor) {
        return pluginProcessor->transportState.isPlaying.load(std::memory_order_relaxed);
    }
    return false;
}

void MIDIXplorerEditor::addLibrary() {
    fileChooser = std::make_unique<juce::FileChooser>(
        "Select MIDI Library Folder",
        juce::File::getSpecialLocation(juce::File::userMusicDirectory),
        ""
    );

    auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories;

    fileChooser->launchAsync(flags, [this](const juce::FileChooser& fc) {
        auto result = fc.getResult();
        if (result.isDirectory()) {
            juce::String newPath = result.getFullPathName();

            // Check if this folder or a parent already exists in libraries
            for (size_t i = 0; i < libraries.size(); i++) {
                juce::String existingPath = libraries[i].path;

                // Check if exact same folder
                if (newPath == existingPath) {
                    // Refresh existing library instead
                    scanLibrary(i);
                    sortFiles();
                    filterFiles();
                    return;
                }

                // Check if new path is a subfolder of existing library
                if (newPath.startsWith(existingPath + "/") || newPath.startsWith(existingPath + "\\")) {
                    // Subfolder of existing - refresh parent library instead
                    scanLibrary(i);
                    sortFiles();
                    filterFiles();
                    return;
                }

                // Check if existing library is a subfolder of new path (new is parent)
                if (existingPath.startsWith(newPath + "/") || existingPath.startsWith(newPath + "\\")) {
                    // Replace the subfolder entry with the parent
                    libraries[i].path = newPath;
                    libraries[i].name = result.getFileName();
                    libraryListBox.updateContent();
                    saveLibraries();
                    scanLibrary(i);
                    sortFiles();
                    filterFiles();
                    return;
                }
            }

            // No duplicates found, add new library
            Library lib;
            lib.name = result.getFileName();
            lib.path = result.getFullPathName();
            lib.enabled = true;
            libraries.push_back(lib);
            libraryListBox.updateContent();
            saveLibraries();
            scanLibrary(libraries.size() - 1);
        }
    });
}

void MIDIXplorerEditor::scanLibraries() {
    allFiles.clear();
    for (size_t i = 0; i < libraries.size(); i++) {
        if (libraries[i].enabled) {
            scanLibrary(i);
        }
    }
    sortFiles();
    filterFiles();
}

void MIDIXplorerEditor::scanLibrary(size_t index) {
    if (index >= libraries.size()) return;

    auto& lib = libraries[index];
    lib.isScanning = true;
    lib.fileCount = 0;

    juce::File dir(lib.path);
    if (!dir.isDirectory()) return;

    auto files = dir.findChildFiles(juce::File::findFiles, true, "*.mid;*.midi");

    // Count total MIDI files in folder (regardless of whether already imported)
    lib.fileCount = (int)files.size();

    for (const auto& file : files) {
        // Check for duplicates - skip if file already exists in library
        bool isDuplicate = false;
        for (const auto& existingFile : allFiles) {
            if (existingFile.fullPath == file.getFullPathName()) {
                isDuplicate = true;
                break;
            }
        }
        if (isDuplicate) continue;

        MIDIFileInfo info;
        info.fileName = file.getFileName();
        info.fullPath = file.getFullPathName();
        info.libraryName = lib.name;
        info.key = "---";
        allFiles.push_back(info);
    }

    lib.isScanning = false;
    libraryListBox.repaint();

    // Analyze files in background (also re-analyze if relativeKey is missing)
    for (size_t i = 0; i < allFiles.size(); i++) {
        if (!allFiles[i].analyzed || allFiles[i].relativeKey.isEmpty()) {
            analyzeFile(i);
        }
    }

    sortFiles();
    filterFiles();
    updateKeyFilterFromDetectedScales();
    
    // Preselect first file so it's ready to play immediately
    if (!filteredFiles.empty() && selectedFileIndex < 0) {
        fileListBox->selectRow(0);
        selectAndPreview(0);
    }
}

void MIDIXplorerEditor::refreshLibrary(size_t index) {
    if (index >= libraries.size()) return;

    auto& lib = libraries[index];

    // Remove all files from this library
    allFiles.erase(
        std::remove_if(allFiles.begin(), allFiles.end(),
            [&lib](const MIDIFileInfo& f) { return f.libraryName == lib.name; }),
        allFiles.end());

    // Rescan the library
    if (lib.enabled) {
        scanLibrary(index);
    }

    libraryListBox.repaint();
}

void MIDIXplorerEditor::analyzeFile(size_t index) {
    if (index >= allFiles.size()) return;

    auto& info = allFiles[index];
    juce::File file(info.fullPath);

    if (!file.existsAsFile()) return;

    // Send all notes off to stop any playing notes from previous file
    if (auto* pluginProcessor = dynamic_cast<MIDIScaleDetector::MIDIScalePlugin*>(&processor)) {
        for (int ch = 1; ch <= 16; ch++) {
            pluginProcessor->addMidiMessage(juce::MidiMessage::allNotesOff(ch));
        }
    }

    juce::FileInputStream stream(file);
    if (!stream.openedOk()) return;

    juce::MidiFile midiFile;
    if (!midiFile.readFrom(stream)) return;

    // Count notes per pitch class
    std::array<int, 12> noteHistogram = {0};

    for (int track = 0; track < midiFile.getNumTracks(); track++) {
        auto* sequence = midiFile.getTrack(track);
        if (sequence) {
            for (int i = 0; i < sequence->getNumEvents(); i++) {
                auto& msg = sequence->getEventPointer(i)->message;
                if (msg.isNoteOn() && msg.getVelocity() > 0) {
                    int pitchClass = msg.getNoteNumber() % 12;
                    noteHistogram[(size_t)pitchClass]++;
                }
            }
        }
    }

    // Comprehensive scale templates (intervals from root)
    struct ScaleTemplate {
        const char* name;
        int intervals[12];
    };
    
    static const ScaleTemplate scales[] = {
        // Major modes
        {"Major",           {1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1}},
        {"Dorian",          {1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 1, 0}},
        {"Phrygian",        {1, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0}},
        {"Lydian",          {1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1}},
        {"Mixolydian",      {1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0}},
        {"Minor",           {1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0}},  // Natural minor/Aeolian
        {"Locrian",         {1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0}},
        
        // Other minor variants
        {"Harmonic Min",    {1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 0, 1}},
        {"Melodic Min",     {1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1}},
        
        // Pentatonic
        {"Major Pent",      {1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0}},
        {"Minor Pent",      {1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0}},
        
        // Blues
        {"Blues",           {1, 0, 0, 1, 0, 1, 1, 1, 0, 0, 1, 0}},
        {"Major Blues",     {1, 0, 1, 1, 1, 0, 0, 1, 0, 1, 0, 0}},
        
        // Exotic/World scales
        {"Hungarian Min",   {1, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 1}},
        {"Spanish",         {1, 1, 0, 0, 1, 1, 0, 1, 1, 0, 1, 0}},
        {"Arabic",          {1, 0, 1, 0, 1, 1, 1, 1, 0, 1, 0, 0}},
        {"Japanese",        {1, 1, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0}},
        {"Egyptian",        {1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0}},
        {"Persian",         {1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 0}},
        
        // Jazz/Bebop
        {"Bebop Dom",       {1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 1}},
        {"Bebop Major",     {1, 0, 1, 0, 1, 1, 0, 1, 1, 1, 0, 1}},
        {"Altered",         {1, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0}},
        
        // Symmetric
        {"Whole Tone",      {1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0}},
        {"Diminished",      {1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1}},
        {"Aug",             {1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1}},
        {"Chromatic",       {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    };
    
    const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    const int numScales = sizeof(scales) / sizeof(scales[0]);
    
    int bestKey = 0;
    int bestScaleIdx = 0;
    int bestScore = -1;

    for (int root = 0; root < 12; root++) {
        for (int s = 0; s < numScales; s++) {
            int score = 0;
            int templateNotes = 0;
            int matchedNotes = 0;
            
            for (int i = 0; i < 12; i++) {
                int idx = (i + root) % 12;
                if (scales[s].intervals[i] == 1) {
                    templateNotes++;
                    score += noteHistogram[(size_t)idx] * 2;  // Bonus for matching scale notes
                    if (noteHistogram[(size_t)idx] > 0) matchedNotes++;
                } else {
                    score -= noteHistogram[(size_t)idx];  // Penalty for non-scale notes
                }
            }
            
            // Prefer scales where more template notes are actually used
            if (templateNotes > 0) {
                score += (matchedNotes * 10) / templateNotes;
            }

            if (score > bestScore) {
                bestScore = score;
                bestKey = root;
                bestScaleIdx = s;
            }
        }
    }

    info.key = juce::String(noteNames[bestKey]) + " " + scales[bestScaleIdx].name;
    
    // Calculate Circle of Fifths - show parent major key and relative minor
    // Each mode has a specific relationship to its parent major scale
    juce::String scaleName = scales[bestScaleIdx].name;
    int parentMajorRoot = bestKey;  // Default: same as detected root
    
    // Calculate parent major key based on mode (semitones from mode root to parent major)
    if (scaleName == "Major" || scaleName == "Major Pent" || scaleName == "Major Blues") {
        parentMajorRoot = bestKey;  // Ionian - root IS the parent major
    } else if (scaleName == "Dorian") {
        parentMajorRoot = (bestKey + 10) % 12;  // 2nd mode: go down 2 semitones (or +10)
    } else if (scaleName == "Phrygian") {
        parentMajorRoot = (bestKey + 8) % 12;   // 3rd mode: go down 4 semitones (or +8)
    } else if (scaleName == "Lydian") {
        parentMajorRoot = (bestKey + 7) % 12;   // 4th mode: go down 5 semitones (or +7)
    } else if (scaleName == "Mixolydian") {
        parentMajorRoot = (bestKey + 5) % 12;   // 5th mode: go down 7 semitones (or +5)
    } else if (scaleName == "Minor" || scaleName == "Minor Pent" || scaleName.containsIgnoreCase("Harmonic") || scaleName.containsIgnoreCase("Melodic")) {
        parentMajorRoot = (bestKey + 3) % 12;   // 6th mode (Aeolian): go up 3 semitones
    } else if (scaleName == "Locrian") {
        parentMajorRoot = (bestKey + 1) % 12;   // 7th mode: go up 1 semitone
    } else if (scaleName == "Blues") {
        parentMajorRoot = (bestKey + 3) % 12;   // Blues is based on minor pentatonic
    } else {
        // For exotic scales, show parallel major/minor relationship
        parentMajorRoot = bestKey;
    }
    
    // Relative minor is always 3 semitones below the parent major (or +9)
    int relativeMinorRoot = (parentMajorRoot + 9) % 12;
    
    // Format: "Parent Maj / Rel m" e.g., "G Maj / Em"
    info.relativeKey = juce::String(noteNames[parentMajorRoot]) + "/" + juce::String(noteNames[relativeMinorRoot]) + "m";
    
    // Extract tempo from MIDI file
    info.bpm = 120.0;  // Default
    for (int track = 0; track < midiFile.getNumTracks(); track++) {
        auto* trackSeq = midiFile.getTrack(track);
        if (trackSeq) {
            for (int i = 0; i < trackSeq->getNumEvents(); i++) {
                auto& msg = trackSeq->getEventPointer(i)->message;
                if (msg.isTempoMetaEvent()) {
                    info.bpm = 60.0 / msg.getTempoSecondsPerQuarterNote();
                    break;
                }
            }
        }
    }

    // Calculate duration
    midiFile.convertTimestampTicksToSeconds();
    double maxTime = 0.0;
    for (int track = 0; track < midiFile.getNumTracks(); track++) {
        auto* sequence = midiFile.getTrack(track);
        if (sequence && sequence->getNumEvents() > 0) {
            double lastTime = sequence->getEventPointer(sequence->getNumEvents() - 1)->message.getTimeStamp();
            if (lastTime > maxTime) maxTime = lastTime;
        }
    }
    info.duration = maxTime;
    // Extract instrument from first program change
    static const char* gmInstruments[] = {
        "Acoustic Grand Piano", "Bright Acoustic Piano", "Electric Grand Piano", "Honky-tonk Piano",
        "Electric Piano 1", "Electric Piano 2", "Harpsichord", "Clavinet",
        "Celesta", "Glockenspiel", "Music Box", "Vibraphone",
        "Marimba", "Xylophone", "Tubular Bells", "Dulcimer",
        "Drawbar Organ", "Percussive Organ", "Rock Organ", "Church Organ",
        "Reed Organ", "Accordion", "Harmonica", "Tango Accordion",
        "Acoustic Guitar (nylon)", "Acoustic Guitar (steel)", "Electric Guitar (jazz)", "Electric Guitar (clean)",
        "Electric Guitar (muted)", "Overdriven Guitar", "Distortion Guitar", "Guitar Harmonics",
        "Acoustic Bass", "Electric Bass (finger)", "Electric Bass (pick)", "Fretless Bass",
        "Slap Bass 1", "Slap Bass 2", "Synth Bass 1", "Synth Bass 2",
        "Violin", "Viola", "Cello", "Contrabass",
        "Tremolo Strings", "Pizzicato Strings", "Orchestral Harp", "Timpani",
        "String Ensemble 1", "String Ensemble 2", "Synth Strings 1", "Synth Strings 2",
        "Choir Aahs", "Voice Oohs", "Synth Voice", "Orchestra Hit",
        "Trumpet", "Trombone", "Tuba", "Muted Trumpet",
        "French Horn", "Brass Section", "Synth Brass 1", "Synth Brass 2",
        "Soprano Sax", "Alto Sax", "Tenor Sax", "Baritone Sax",
        "Oboe", "English Horn", "Bassoon", "Clarinet",
        "Piccolo", "Flute", "Recorder", "Pan Flute",
        "Blown Bottle", "Shakuhachi", "Whistle", "Ocarina",
        "Lead 1 (square)", "Lead 2 (sawtooth)", "Lead 3 (calliope)", "Lead 4 (chiff)",
        "Lead 5 (charang)", "Lead 6 (voice)", "Lead 7 (fifths)", "Lead 8 (bass + lead)",
        "Pad 1 (new age)", "Pad 2 (warm)", "Pad 3 (polysynth)", "Pad 4 (choir)",
        "Pad 5 (bowed)", "Pad 6 (metallic)", "Pad 7 (halo)", "Pad 8 (sweep)",
        "FX 1 (rain)", "FX 2 (soundtrack)", "FX 3 (crystal)", "FX 4 (atmosphere)",
        "FX 5 (brightness)", "FX 6 (goblins)", "FX 7 (echoes)", "FX 8 (sci-fi)",
        "Sitar", "Banjo", "Shamisen", "Koto",
        "Kalimba", "Bagpipe", "Fiddle", "Shanai",
        "Tinkle Bell", "Agogo", "Steel Drums", "Woodblock",
        "Taiko Drum", "Melodic Tom", "Synth Drum", "Reverse Cymbal",
        "Guitar Fret Noise", "Breath Noise", "Seashore", "Bird Tweet",
        "Telephone Ring", "Helicopter", "Applause", "Gunshot"
    };
    
    info.instrument = "---";
    for (int track = 0; track < midiFile.getNumTracks(); track++) {
        auto* trackSeq = midiFile.getTrack(track);
        if (trackSeq) {
            for (int i = 0; i < trackSeq->getNumEvents(); i++) {
                auto& msg = trackSeq->getEventPointer(i)->message;
                if (msg.isProgramChange()) {
                    int program = msg.getProgramChangeNumber();
                    if (program >= 0 && program < 128) {
                        info.instrument = gmInstruments[program];
                    }
                    break;
                }
            }
        }
        if (info.instrument != "---") break;
    }
    
    info.analyzed = true;
}

void MIDIXplorerEditor::sortFiles() {
    int sortOption = sortCombo.getSelectedId();

    if (sortOption == 1) {
        // Sort by Scale/Key
        std::sort(allFiles.begin(), allFiles.end(), [](const MIDIFileInfo& a, const MIDIFileInfo& b) {
            int orderA = getKeyOrder(a.key);
            int orderB = getKeyOrder(b.key);
            if (orderA != orderB) return orderA < orderB;
            return a.fileName.compareIgnoreCase(b.fileName) < 0;
        });
    } else if (sortOption == 2) {
        // Sort by Duration
        std::sort(allFiles.begin(), allFiles.end(), [](const MIDIFileInfo& a, const MIDIFileInfo& b) {
            if (a.duration != b.duration) return a.duration < b.duration;
            return a.fileName.compareIgnoreCase(b.fileName) < 0;
        });
    } else {
        // Sort by Name
        std::sort(allFiles.begin(), allFiles.end(), [](const MIDIFileInfo& a, const MIDIFileInfo& b) {
            return a.fileName.compareIgnoreCase(b.fileName) < 0;
        });
    }
}

void MIDIXplorerEditor::filterFiles() {
    filteredFiles.clear();

    juce::String keyFilter = keyFilterCombo.getText();
    juce::String searchText = searchBox.getText().toLowerCase();

    for (const auto& file : allFiles) {
        // Check favorites filter (ID 2)
        if (keyFilterCombo.getSelectedId() == 2) {
            if (!file.favorite) continue;
        }
        // Check key filter (IDs > 2)
        else if (keyFilterCombo.getSelectedId() > 2) {
            if (file.key != keyFilter) continue;
        }

        // Check search filter
        if (searchText.isNotEmpty()) {
            if (!file.fileName.toLowerCase().contains(searchText) &&
                !file.key.toLowerCase().contains(searchText)) {
                continue;
            }
        }

        filteredFiles.push_back(file);
    }

    fileCountLabel.setText(juce::String((int)filteredFiles.size()) + " files", juce::dontSendNotification);
    fileListBox->updateContent();
    fileListBox->repaint();
}

void MIDIXplorerEditor::updateKeyFilterFromDetectedScales() {
    detectedKeys.clear();
    juce::StringArray uniqueKeys;

    for (const auto& file : allFiles) {
        if (file.key != "---" && !uniqueKeys.contains(file.key)) {
            uniqueKeys.add(file.key);
        }
    }

    // Bubble sort by musical key order (JUCE StringArray doesn't support custom comparator)
    for (int i = 0; i < uniqueKeys.size() - 1; i++) {
        for (int j = i + 1; j < uniqueKeys.size(); j++) {
            if (getKeyOrder(uniqueKeys[i]) > getKeyOrder(uniqueKeys[j])) {
                uniqueKeys.getReference(i).swapWith(uniqueKeys.getReference(j));
            }
        }
    }

    keyFilterCombo.clear();
    keyFilterCombo.addItem("All Keys", 1);
    keyFilterCombo.addItem("♥ Favorites", 2);

    int id = 3;
    for (const auto& key : uniqueKeys) {
        keyFilterCombo.addItem(key, id++);
    }

    keyFilterCombo.setSelectedId(1);
}

void MIDIXplorerEditor::revealInFinder(const juce::String& path) {
    juce::File(path).revealToUser();
}

void MIDIXplorerEditor::toggleFavorite(int row) {
    if (row < 0 || row >= (int)filteredFiles.size()) return;
    
    auto& filteredFile = filteredFiles[(size_t)row];
    filteredFile.favorite = !filteredFile.favorite;
    
    // Also update in allFiles
    for (auto& f : allFiles) {
        if (f.fullPath == filteredFile.fullPath) {
            f.favorite = filteredFile.favorite;
            break;
        }
    }
    
    // Save favorites
    saveFavorites();
    
    // Refresh list
    fileListBox->repaint();
}

void MIDIXplorerEditor::saveFavorites() {
    auto settingsDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("MIDIXplorer");
    auto favFile = settingsDir.getChildFile("favorites.txt");
    
    juce::String content;
    for (const auto& f : allFiles) {
        if (f.favorite) {
            content += f.fullPath + "\n";
        }
    }
    favFile.replaceWithText(content);
}

void MIDIXplorerEditor::loadFavorites() {
    auto settingsDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("MIDIXplorer");
    auto favFile = settingsDir.getChildFile("favorites.txt");
    
    if (!favFile.existsAsFile()) return;
    
    juce::StringArray favPaths;
    favFile.readLines(favPaths);
    
    for (auto& f : allFiles) {
        f.favorite = favPaths.contains(f.fullPath);
    }
}

// Library list model implementation

// MIDI Note Viewer implementation
void MIDIXplorerEditor::MIDINoteViewer::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds();
    
    // Background
    g.setColour(juce::Colour(0xff1e1e1e));
    g.fillRect(bounds);
    
    // Border
    g.setColour(juce::Colour(0xff3a3a3a));
    g.drawRect(bounds);
    
    if (sequence == nullptr || sequence->getNumEvents() == 0) {
        g.setColour(juce::Colours::grey);
        g.drawText("No MIDI loaded", bounds, juce::Justification::centred);
        return;
    }
    
    int noteRange = highestNote - lowestNote + 1;
    if (noteRange <= 0) noteRange = 1;
    
    float noteHeight = (float)bounds.getHeight() / (float)noteRange;
    float pixelsPerSecond = (float)bounds.getWidth() / (float)totalDuration;
    
    // Draw piano key background hints
    for (int note = lowestNote; note <= highestNote; note++) {
        float y = bounds.getHeight() - (note - lowestNote + 1) * noteHeight;
        int noteInOctave = note % 12;
        bool isBlackKey = (noteInOctave == 1 || noteInOctave == 3 || noteInOctave == 6 || noteInOctave == 8 || noteInOctave == 10);
        if (isBlackKey) {
            g.setColour(juce::Colour(0xff161616));
            g.fillRect(0.0f, y, (float)bounds.getWidth(), noteHeight);
        }
    }
    
    // Draw notes
    for (int i = 0; i < sequence->getNumEvents(); i++) {
        auto* event = sequence->getEventPointer(i);
        auto& msg = event->message;
        
        if (msg.isNoteOn()) {
            int noteNum = msg.getNoteNumber();
            double startTime = msg.getTimeStamp();
            double endTime = startTime + 0.1; // Default duration
            
            // Find matching note-off
            for (int j = i + 1; j < sequence->getNumEvents(); j++) {
                auto* offEvent = sequence->getEventPointer(j);
                if (offEvent->message.isNoteOff() && offEvent->message.getNoteNumber() == noteNum) {
                    endTime = offEvent->message.getTimeStamp();
                    break;
                }
            }
            
            float x = (float)(startTime * pixelsPerSecond);
            float w = (float)((endTime - startTime) * pixelsPerSecond);
            if (w < 2.0f) w = 2.0f;
            float y = bounds.getHeight() - (noteNum - lowestNote + 1) * noteHeight;
            
            // Note color based on velocity
            int velocity = msg.getVelocity();
            float brightness = 0.5f + (velocity / 254.0f) * 0.5f;
            g.setColour(juce::Colour::fromHSV(0.55f, 0.7f, brightness, 1.0f));
            g.fillRoundedRectangle(x, y + 1, w, noteHeight - 2, 2.0f);
        }
    }
    
    // Draw playhead
    if (playPosition >= 0 && playPosition <= 1.0) {
        float xPos = playPosition * bounds.getWidth();
        g.setColour(juce::Colours::white);
        g.drawVerticalLine((int)xPos, 0.0f, (float)bounds.getHeight());
    }
}

void MIDIXplorerEditor::MIDINoteViewer::setSequence(const juce::MidiMessageSequence* seq, double duration) {
    sequence = seq;
    totalDuration = duration > 0 ? duration : 1.0;
    
    // Calculate note range
    lowestNote = 127;
    highestNote = 0;
    if (seq != nullptr) {
        for (int i = 0; i < seq->getNumEvents(); i++) {
            auto& msg = seq->getEventPointer(i)->message;
            if (msg.isNoteOn()) {
                int note = msg.getNoteNumber();
                if (note < lowestNote) lowestNote = note;
                if (note > highestNote) highestNote = note;
            }
        }
    }
    // Add some padding
    if (lowestNote > 0) lowestNote--;
    if (highestNote < 127) highestNote++;
    if (lowestNote > highestNote) { lowestNote = 60; highestNote = 72; }
    
    repaint();
}

void MIDIXplorerEditor::MIDINoteViewer::setPlaybackPosition(double position) {
    playPosition = position;
    repaint();
}
void MIDIXplorerEditor::LibraryListModel::paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) {
    if (row < 0 || row >= (int)owner.libraries.size()) return;

    auto& lib = owner.libraries[(size_t)row];

    if (selected) {
        g.setColour(juce::Colour(0xff3a3a3a));
        g.fillRect(0, 0, w, h);
    }

    g.setColour(lib.enabled ? juce::Colours::white : juce::Colours::grey);
    g.setFont(13.0f);
    g.drawText(lib.name, 8, 0, w - 60, h, juce::Justification::centredLeft);

    g.setColour(juce::Colours::grey);
    g.setFont(11.0f);
    g.drawText(juce::String(lib.fileCount), w - 50, 0, 45, h, juce::Justification::centredRight);
}

void MIDIXplorerEditor::LibraryListModel::listBoxItemClicked(int row, const juce::MouseEvent& e) {
    if (row < 0 || row >= (int)owner.libraries.size()) return;

    if (e.mods.isRightButtonDown()) {
        juce::PopupMenu menu;
        menu.addItem(1, "Reveal in Finder");
        menu.addItem(2, "Refresh");
        menu.addItem(3, owner.libraries[(size_t)row].enabled ? "Disable" : "Enable");
        menu.addItem(4, "Remove");

        menu.showMenuAsync(juce::PopupMenu::Options(), [this, row](int result) {
            if (result == 1) {
                owner.revealInFinder(owner.libraries[(size_t)row].path);
            } else if (result == 2) {
                owner.refreshLibrary((size_t)row);
            } else if (result == 3) {
                owner.libraries[(size_t)row].enabled = !owner.libraries[(size_t)row].enabled;
                owner.saveLibraries();
                owner.scanLibraries();
            } else if (result == 4) {
                owner.libraries.erase(owner.libraries.begin() + row);
                owner.saveLibraries();
                owner.libraryListBox.updateContent();
                owner.scanLibraries();
            }
        });
    }
}

// File list model implementation
void MIDIXplorerEditor::FileListModel::paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) {
    if (row < 0 || row >= (int)owner.filteredFiles.size()) return;

    auto& file = owner.filteredFiles[(size_t)row];

    if (selected) {
        g.setColour(juce::Colour(0xff0078d4));
        g.fillRect(0, 0, w, h);
    } else if (row % 2 == 0) {
        g.setColour(juce::Colour(0xff242424));
        g.fillRect(0, 0, w, h);
    }

    // Heart favorite icon
    g.setFont(16.0f);
    if (file.favorite) {
        g.setColour(juce::Colour(0xffff4466));  // Red heart for favorites
        g.drawText(juce::String::fromUTF8("♥"), 4, 0, 20, h, juce::Justification::centred);
    } else {
        g.setColour(juce::Colour(0xff666666));  // Grey heart outline
        g.drawText(juce::String::fromUTF8("♡"), 4, 0, 20, h, juce::Justification::centred);
    }

    // Key badge
    g.setColour(juce::Colour(0xff3a3a3a));
    g.fillRoundedRectangle(28.0f, 6.0f, 70.0f, 20.0f, 4.0f);
    g.setColour(juce::Colours::cyan);
    g.setFont(11.0f);
    g.drawText(file.key, 28, 6, 70, 20, juce::Justification::centred);
    
    // Circle of Fifths relative key (Parent Major / Relative minor)
    if (file.relativeKey.isNotEmpty()) {
        g.setColour(juce::Colour(0xffff9944));  // Orange for relative key
        g.setFont(9.0f);
        g.drawText("(" + file.relativeKey + ")", 102, 6, 55, 20, juce::Justification::centredLeft);
    }

    // File name
    g.setColour(juce::Colours::white);
    g.setFont(13.0f);
    g.drawText(file.fileName, 160, 0, w - 415, h, juce::Justification::centredLeft);

    // Instrument name
    g.setColour(juce::Colour(0xffaaaaff));
    g.drawText(file.instrument, w - 330, 0, 140, h, juce::Justification::centredLeft);

    // Duration in seconds
    g.setColour(juce::Colours::grey);
    g.setFont(11.0f);
    // Draw BPM
    juce::String bpmStr = juce::String((int)file.bpm) + " bpm";
    g.drawText(bpmStr, w - 130, 0, 60, h, juce::Justification::centredRight);

    // Draw duration
    int mins = (int)(file.duration) / 60;
    int secs = (int)(file.duration) % 60;
    juce::String durationStr = juce::String::formatted("%d:%02d", mins, secs);
    g.drawText(durationStr, w - 60, 0, 55, h, juce::Justification::centredRight);

    // Progress bar for currently playing file
    if (row == owner.selectedFileIndex && owner.isPlaying && owner.fileLoaded) {
        // Background of progress bar
        g.setColour(juce::Colour(0xff333333));
        g.fillRect(8, h - 4, w - 16, 3);
        // Progress fill
        int progressWidth = (int)((w - 16) * owner.currentPlaybackPosition);
        g.setColour(juce::Colour(0xff00cc88));
        g.fillRect(8, h - 4, progressWidth, 3);
    }
}

void MIDIXplorerEditor::FileListModel::selectedRowsChanged(int lastRowSelected) {
    // Trigger preview when selection changes (arrow keys, click, etc.)
    if (lastRowSelected >= 0 && lastRowSelected < (int)owner.filteredFiles.size()) {
        owner.selectAndPreview(lastRowSelected);
        return;
    }
}

juce::var MIDIXplorerEditor::FileListModel::getDragSourceDescription(const juce::SparseSet<int>& selectedRows) {
    if (selectedRows.isEmpty()) return {};

    int row = selectedRows[0];
    if (row >= 0 && row < (int)owner.filteredFiles.size()) {
        return owner.filteredFiles[(size_t)row].fullPath;
    }
    return {};
}

void MIDIXplorerEditor::FileListModel::listBoxItemClicked(int row, const juce::MouseEvent& e) {
    if (row < 0 || row >= (int)owner.filteredFiles.size()) return;

    // Check if heart area clicked (first 24 pixels)
    if (e.x < 24 && e.mods.isLeftButtonDown()) {
        owner.toggleFavorite(row);
        return;
    }

    if (e.mods.isRightButtonDown()) {
        juce::PopupMenu menu;
        menu.addItem(1, "Reveal in Finder");

        menu.showMenuAsync(juce::PopupMenu::Options(), [this, row](int result) {
            if (result == 1) {
                owner.revealInFinder(owner.filteredFiles[(size_t)row].fullPath);
            }
        });
    } else {
        // Left click - if clicking same row, restart playback from beginning
        if (row == owner.fileListBox->getSelectedRow()) {
            owner.restartPlayback();
        }
        // Otherwise selectedRowsChanged handles preview
    }
}
