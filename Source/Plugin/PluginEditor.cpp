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

    // Search box
    searchBox.setTextToShowWhenEmpty("Search files...", juce::Colours::grey);
    searchBox.onTextChange = [this]() { filterFiles(); };
    addAndMakeVisible(searchBox);

    // File list with custom draggable listbox
    fileModel = std::make_unique<FileListModel>(*this);
    fileListBox = std::make_unique<DraggableListBox>("Files", *this);
    fileListBox->setModel(fileModel.get());
    fileListBox->setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff1e1e1e));
    fileListBox->setRowHeight(32);
    fileListBox->setMultipleSelectionEnabled(false);
    addAndMakeVisible(fileListBox.get());

    // Transport controls - Play button
    playButton.setButtonText(juce::String::fromUTF8("\u25B6"));  // Play symbol
    playButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3a3a3a));
    playButton.setColour(juce::TextButton::textColourOffId, juce::Colours::limegreen);
    playButton.onClick = [this]() {
        isPlaying = true;
        // Reset playback to beginning if stopped
        if (playbackNoteIndex == 0 && fileLoaded) {
            playbackStartTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
            playbackStartBeat = getHostBeatPosition();
        }
    };
    addAndMakeVisible(playButton);

    // Pause button
    pauseButton.setButtonText(juce::String::fromUTF8("\u23F8"));  // Pause symbol
    pauseButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3a3a3a));
    pauseButton.setColour(juce::TextButton::textColourOffId, juce::Colours::orange);
    pauseButton.onClick = [this]() {
        isPlaying = false;
        // Send all notes off when pausing
        if (pluginProcessor) {
            for (int ch = 1; ch <= 16; ch++) {
                pluginProcessor->addMidiMessage(juce::MidiMessage::allNotesOff(ch));
            }
        }
    };
    addAndMakeVisible(pauseButton);

    // Stop button
    stopButton.setButtonText(juce::String::fromUTF8("\u23F9"));  // Stop symbol
    stopButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3a3a3a));
    stopButton.setColour(juce::TextButton::textColourOffId, juce::Colours::red);
    stopButton.onClick = [this]() {
        isPlaying = false;
        playbackNoteIndex = 0;
        // Send all notes off
        if (pluginProcessor) {
            for (int ch = 1; ch <= 16; ch++) {
                pluginProcessor->addMidiMessage(juce::MidiMessage::allNotesOff(ch));
            }
        }
        // Reset playback position
        playbackStartTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        playbackStartBeat = getHostBeatPosition();
    };
    addAndMakeVisible(stopButton);

    syncToHostToggle.setToggleState(true, juce::dontSendNotification);
    syncToHostToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    syncToHostToggle.setColour(juce::ToggleButton::tickColourId, juce::Colours::orange);
    addAndMakeVisible(syncToHostToggle);

    transportSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    transportSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    transportSlider.setRange(0.0, 1.0);
    transportSlider.setColour(juce::Slider::thumbColourId, juce::Colours::cyan);
    transportSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xff3a3a3a));
    addAndMakeVisible(transportSlider);


    timeDisplayLabel.setFont(juce::Font(14.0f));
    timeDisplayLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    timeDisplayLabel.setJustificationType(juce::Justification::centredRight);
    timeDisplayLabel.setText("0:00 / 0:00", juce::dontSendNotification);
    addAndMakeVisible(timeDisplayLabel);
    setSize(700, 500);

    // Load saved libraries
    loadLibraries();

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

    // Top bar with key filter and search
    auto topBar = area.removeFromTop(36);
    topBar = topBar.reduced(8, 4);
    fileCountLabel.setBounds(topBar.removeFromLeft(80));
    keyFilterCombo.setBounds(topBar.removeFromLeft(100).reduced(2));
    topBar.removeFromLeft(8);
    syncToHostToggle.setBounds(topBar.removeFromRight(90));
    topBar.removeFromRight(8);
    searchBox.setBounds(topBar);

    // Bottom transport bar
    auto transport = area.removeFromBottom(40).reduced(8, 4);
    playButton.setBounds(transport.removeFromLeft(36));
    pauseButton.setBounds(transport.removeFromLeft(36));
    stopButton.setBounds(transport.removeFromLeft(36));
    transport.removeFromLeft(10);
    timeDisplayLabel.setBounds(transport.removeFromRight(80));
    transportSlider.setBounds(transport);

    // File list fills the rest
    fileListBox->setBounds(area.reduced(4));
}

void MIDIXplorerEditor::timerCallback() {
    if (!isPlaying || !fileLoaded) {
        return;
    }

    bool synced = syncToHostToggle.getToggleState();
    bool hostPlaying = isHostPlaying();
    double hostBpm = getHostBpm();
    double hostBeat = getHostBeatPosition();

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

    // Handle host play/stop changes
    if (synced) {
        if (hostPlaying && !wasHostPlaying) {
            // Host just started playing - start playback immediately from beat 0
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
        }
        wasHostPlaying = hostPlaying;

        // When host is stopped in sync mode, switch to freerun for browsing
        if (!hostPlaying) {
            synced = false;  // Use freerun timing when host is stopped
        }
    }

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
        double overshoot = currentTime - totalDuration;
        currentTime = overshoot;
        playbackNoteIndex = 0;

        if (synced) {
            // Calculate how many beats the file duration represents
            double fileDurationInBeats = (totalDuration * midiFileBpm) / 60.0;
            // Advance start beat by file duration in beats (preserves fractional timing)
            playbackStartBeat += fileDurationInBeats;
        } else {
            playbackStartTime = juce::Time::getMillisecondCounterHiRes() / 1000.0 - overshoot;
        }
        // Continue to play notes that should have started
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
            Library lib;
            lib.name = result.getFileName();
            lib.path = result.getFullPathName();
            lib.enabled = true;
            libraries.push_back(lib);
            libraryListBox.updateContent();
            saveLibraries(); // Save immediately
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
    sortFilesByKey();
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
        lib.fileCount++;
    }

    lib.isScanning = false;
    libraryListBox.repaint();

    // Analyze files in background
    for (size_t i = 0; i < allFiles.size(); i++) {
        if (!allFiles[i].analyzed) {
            analyzeFile(i);
        }
    }

    sortFilesByKey();
    filterFiles();
    updateKeyFilterFromDetectedScales();
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

    // Major and minor scale templates
    const int majorTemplate[] = {1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1};
    const int minorTemplate[] = {1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0};
    const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

    int bestKey = 0;
    bool bestIsMajor = true;
    int bestScore = -1;

    for (int root = 0; root < 12; root++) {
        int majorScore = 0, minorScore = 0;
        for (int i = 0; i < 12; i++) {
            int idx = (i + root) % 12;
            majorScore += noteHistogram[(size_t)idx] * majorTemplate[i];
            minorScore += noteHistogram[(size_t)idx] * minorTemplate[i];
        }

        if (majorScore > bestScore) {
            bestScore = majorScore;
            bestKey = root;
            bestIsMajor = true;
        }
        if (minorScore > bestScore) {
            bestScore = minorScore;
            bestKey = root;
            bestIsMajor = false;
        }
    }

    info.key = juce::String(noteNames[bestKey]) + (bestIsMajor ? " Major" : " Minor");
    info.analyzed = true;
}

void MIDIXplorerEditor::sortFilesByKey() {
    std::sort(allFiles.begin(), allFiles.end(), [](const MIDIFileInfo& a, const MIDIFileInfo& b) {
        int orderA = getKeyOrder(a.key);
        int orderB = getKeyOrder(b.key);
        if (orderA != orderB) return orderA < orderB;
        return a.fileName.compareIgnoreCase(b.fileName) < 0;
    });
}

void MIDIXplorerEditor::filterFiles() {
    filteredFiles.clear();

    juce::String keyFilter = keyFilterCombo.getText();
    juce::String searchText = searchBox.getText().toLowerCase();

    for (const auto& file : allFiles) {
        // Check key filter
        if (keyFilterCombo.getSelectedId() > 1) {
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

    int id = 2;
    for (const auto& key : uniqueKeys) {
        keyFilterCombo.addItem(key, id++);
    }

    keyFilterCombo.setSelectedId(1);
}

void MIDIXplorerEditor::revealInFinder(const juce::String& path) {
    juce::File(path).revealToUser();
}

// Library list model implementation
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
        menu.addItem(2, owner.libraries[(size_t)row].enabled ? "Disable" : "Enable");
        menu.addItem(3, "Remove");

        menu.showMenuAsync(juce::PopupMenu::Options(), [this, row](int result) {
            if (result == 1) {
                owner.revealInFinder(owner.libraries[(size_t)row].path);
            } else if (result == 2) {
                owner.libraries[(size_t)row].enabled = !owner.libraries[(size_t)row].enabled;
                owner.saveLibraries();
                owner.scanLibraries();
            } else if (result == 3) {
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

    // Key badge
    g.setColour(juce::Colour(0xff3a3a3a));
    g.fillRoundedRectangle(8.0f, 6.0f, 70.0f, 20.0f, 4.0f);
    g.setColour(juce::Colours::cyan);
    g.setFont(11.0f);
    g.drawText(file.key, 8, 6, 70, 20, juce::Justification::centred);

    // File name
    g.setColour(juce::Colours::white);
    g.setFont(13.0f);
    g.drawText(file.fileName, 88, 0, w - 180, h, juce::Justification::centredLeft);

    // Library name
    g.setColour(juce::Colours::grey);
    g.setFont(11.0f);
    g.drawText(file.libraryName, w - 90, 0, 85, h, juce::Justification::centredRight);
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

    if (e.mods.isRightButtonDown()) {
        juce::PopupMenu menu;
        menu.addItem(1, "Reveal in Finder");
        menu.addItem(2, "Preview");

        menu.showMenuAsync(juce::PopupMenu::Options(), [this, row](int result) {
            if (result == 1) {
                owner.revealInFinder(owner.filteredFiles[(size_t)row].fullPath);
            } else if (result == 2) {
                // selectedRowsChanged handles preview
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
