#include "PluginEditor.h"
#include "MIDIScalePlugin.h"

static const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

static juce::String detectKey(const juce::MidiFile& midiFile) {
    std::array<int, 12> noteHistogram = {0};
    int noteCount = 0;
    
    for (int track = 0; track < midiFile.getNumTracks(); ++track) {
        auto* seq = midiFile.getTrack(track);
        if (!seq) continue;
        for (int i = 0; i < seq->getNumEvents(); ++i) {
            auto& msg = seq->getEventPointer(i)->message;
            if (msg.isNoteOn()) {
                noteHistogram[(size_t)(msg.getNoteNumber() % 12)]++;
                noteCount++;
            }
        }
    }
    
    if (noteCount == 0) return "?";
    
    const int majorP[] = {0, 2, 4, 5, 7, 9, 11};
    const int minorP[] = {0, 2, 3, 5, 7, 8, 10};
    
    int bestScore = 0, bestRoot = 0;
    bool isMajor = true;
    
    for (int root = 0; root < 12; ++root) {
        int majScore = 0, minScore = 0;
        for (int i = 0; i < 7; ++i) {
            majScore += noteHistogram[(size_t)((root + majorP[i]) % 12)];
            minScore += noteHistogram[(size_t)((root + minorP[i]) % 12)];
        }
        if (majScore > bestScore) { bestScore = majScore; bestRoot = root; isMajor = true; }
        if (minScore > bestScore) { bestScore = minScore; bestRoot = root; isMajor = false; }
    }
    
    return juce::String(noteNames[bestRoot]) + (isMajor ? " Major" : "min");
}

// Library List Model
void MIDIXplorerEditor::LibraryListModel::paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) {
    if (row < 0 || row >= (int)owner.libraries.size()) return;
    auto& lib = owner.libraries[(size_t)row];
    
    g.fillAll(selected ? juce::Colour(0xFFD0D0D0) : juce::Colours::white);
    
    g.setColour(juce::Colours::darkgrey);
    g.drawRect(4, h/2 - 6, 12, 12);
    if (lib.enabled) {
        g.setColour(juce::Colours::black);
        g.drawLine(6.0f, (float)(h/2), 9.0f, (float)(h/2 + 3), 2.0f);
        g.drawLine(9.0f, (float)(h/2 + 3), 14.0f, (float)(h/2 - 4), 2.0f);
    }
    
    g.setColour(juce::Colours::black);
    g.drawText(lib.name, 22, 0, w - 70, h, juce::Justification::left);
    
    if (lib.isScanning) {
        g.setColour(juce::Colours::orange);
        g.drawText("...", w - 45, 0, 40, h, juce::Justification::right);
    } else {
        g.setColour(juce::Colours::grey);
        g.drawText(juce::String(lib.fileCount), w - 40, 0, 35, h, juce::Justification::right);
    }
}

void MIDIXplorerEditor::LibraryListModel::listBoxItemClicked(int row, const juce::MouseEvent& e) {
    if (row < 0 || row >= (int)owner.libraries.size()) return;
    
    if (e.x < 20) {
        owner.libraries[(size_t)row].enabled = !owner.libraries[(size_t)row].enabled;
        owner.libraryListBox.repaint();
        owner.filterFiles();
    }
    else if (e.mods.isRightButtonDown()) {
        juce::PopupMenu menu;
        menu.addItem(1, "Reveal in Finder");
        menu.addItem(2, "Remove Library");
        
        menu.showMenuAsync(juce::PopupMenu::Options(), [this, row](int result) {
            if (result == 1) {
                owner.revealInFinder(owner.libraries[(size_t)row].path);
            } else if (result == 2) {
                owner.libraries.erase(owner.libraries.begin() + row);
                owner.libraryListBox.updateContent();
                owner.scanLibraries();
            }
        });
    }
}

// File List Model
void MIDIXplorerEditor::FileListModel::paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) {
    if (row < 0 || row >= (int)owner.filteredFiles.size()) return;
    auto& file = owner.filteredFiles[(size_t)row];
    
    g.fillAll(selected ? juce::Colour(0xFFE8E8E8) : (row % 2 == 0 ? juce::Colours::white : juce::Colour(0xFFF8F8F8)));
    
    g.setColour(juce::Colours::black);
    g.drawText(file.fileName, 10, 0, w - 120, h, juce::Justification::left);
    
    if (file.analyzed) {
        g.setColour(juce::Colour(0xFF4080C0));
        g.drawText(file.key, w - 100, 0, 90, h, juce::Justification::right);
    } else {
        g.setColour(juce::Colours::grey);
        g.drawText("...", w - 100, 0, 90, h, juce::Justification::right);
    }
}

void MIDIXplorerEditor::FileListModel::selectedRowsChanged(int lastRowSelected) {
    owner.selectedFileIndex = lastRowSelected;
    if (owner.previewToggle.getToggleState() && lastRowSelected >= 0) {
        owner.scheduleFileChange();
    }
}

void MIDIXplorerEditor::FileListModel::listBoxItemClicked(int row, const juce::MouseEvent& e) {
    if (e.mods.isRightButtonDown() && row >= 0 && row < (int)owner.filteredFiles.size()) {
        juce::PopupMenu menu;
        menu.addItem(1, "Reveal in Finder");
        menu.showMenuAsync(juce::PopupMenu::Options(), [this, row](int result) {
            if (result == 1) {
                owner.revealInFinder(owner.filteredFiles[(size_t)row].fullPath);
            }
        });
    }
}

juce::var MIDIXplorerEditor::FileListModel::getDragSourceDescription(const juce::SparseSet<int>& selectedRows) {
    if (selectedRows.isEmpty()) return {};
    int row = selectedRows[0];
    if (row >= 0 && row < (int)owner.filteredFiles.size()) {
        return juce::var(owner.filteredFiles[(size_t)row].fullPath);
    }
    return {};
}

// Main Editor
MIDIXplorerEditor::MIDIXplorerEditor(juce::AudioProcessor& p) : AudioProcessorEditor(&p) {
    pluginProcessor = dynamic_cast<MIDIScaleDetector::MIDIScalePlugin*>(&p);
    
    libraryModel = std::make_unique<LibraryListModel>(*this);
    fileModel = std::make_unique<FileListModel>(*this);
    fileListBox = std::make_unique<DraggableListBox>("Files", *this);
    
    addLibraryBtn.onClick = [this]() { addLibrary(); };
    addAndMakeVisible(addLibraryBtn);
    
    librariesLabel.setText("Libraries", juce::dontSendNotification);
    librariesLabel.setFont(juce::FontOptions(14.0f));
    librariesLabel.setColour(juce::Label::textColourId, juce::Colours::black);
    addAndMakeVisible(librariesLabel);
    
    libraryListBox.setModel(libraryModel.get());
    libraryListBox.setRowHeight(22);
    libraryListBox.setColour(juce::ListBox::backgroundColourId, juce::Colours::white);
    addAndMakeVisible(libraryListBox);
    
    fileCountLabel.setText("Name (0 files)", juce::dontSendNotification);
    fileCountLabel.setColour(juce::Label::textColourId, juce::Colours::black);
    addAndMakeVisible(fileCountLabel);
    
    keyFilterCombo.addItem("All Keys", 1);
    keyFilterCombo.setSelectedId(1);
    keyFilterCombo.onChange = [this]() { filterFiles(); };
    addAndMakeVisible(keyFilterCombo);
    
    searchBox.setTextToShowWhenEmpty("Search Midi Files", juce::Colours::grey);
    searchBox.onTextChange = [this]() { filterFiles(); };
    addAndMakeVisible(searchBox);
    
    fileListBox->setModel(fileModel.get());
    fileListBox->setRowHeight(20);
    fileListBox->setColour(juce::ListBox::backgroundColourId, juce::Colours::white);
    addAndMakeVisible(fileListBox.get());
    
    previewToggle.setToggleState(false, juce::dontSendNotification);
    previewToggle.onClick = [this]() {
        if (!previewToggle.getToggleState()) {
            stopPlayback();
        } else if (selectedFileIndex >= 0) {
            loadSelectedFile();
        }
    };
    addAndMakeVisible(previewToggle);
    
    syncToHostToggle.setToggleState(true, juce::dontSendNotification);
    syncToHostToggle.setTooltip("Sync playback to DAW - starts when DAW plays");
    addAndMakeVisible(syncToHostToggle);
    
    transportSlider.setRange(0, 1, 0.01);
    transportSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    transportSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    addAndMakeVisible(transportSlider);
    
    setWantsKeyboardFocus(true);
    startTimer(5);
    setSize(900, 550);
}

MIDIXplorerEditor::~MIDIXplorerEditor() {
    stopTimer();
    stopPlayback();
}

bool MIDIXplorerEditor::keyPressed(const juce::KeyPress& key) {
    if (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey) {
        int newRow = selectedFileIndex;
        
        if (key == juce::KeyPress::upKey) {
            newRow = std::max(0, selectedFileIndex - 1);
        } else if (key == juce::KeyPress::downKey) {
            newRow = std::min((int)filteredFiles.size() - 1, selectedFileIndex + 1);
        }
        
        if (newRow != selectedFileIndex && newRow >= 0 && newRow < (int)filteredFiles.size()) {
            selectAndPreview(newRow);
            return true;
        }
    }
    return false;
}

void MIDIXplorerEditor::selectAndPreview(int row) {
    if (row < 0 || row >= (int)filteredFiles.size()) return;
    
    selectedFileIndex = row;
    fileListBox->selectRow(row);
    fileListBox->scrollToEnsureRowIsOnscreen(row);
    
    if (previewToggle.getToggleState()) {
        scheduleFileChange();
    }
}

void MIDIXplorerEditor::scheduleFileChange() {
    // Mark that we need to change file on next beat
    pendingFileChange = true;
    pendingFileIndex = selectedFileIndex;
    
    // If not synced or host not playing, change immediately
    if (!syncToHostToggle.getToggleState() || !isHostPlaying()) {
        loadSelectedFile();
        pendingFileChange = false;
    }
}

void MIDIXplorerEditor::loadSelectedFile() {
    if (selectedFileIndex < 0 || selectedFileIndex >= (int)filteredFiles.size()) return;
    if (!pluginProcessor) return;
    
    // Stop current notes
    for (int ch = 1; ch <= 16; ++ch) {
        pluginProcessor->addMidiMessage(juce::MidiMessage::allNotesOff(ch));
    }
    
    juce::File file(filteredFiles[(size_t)selectedFileIndex].fullPath);
    juce::FileInputStream stream(file);
    
    if (!stream.openedOk()) return;
    
    juce::MidiFile tempMidiFile;
    if (!tempMidiFile.readFrom(stream)) return;
    
    // Get tempo before converting
    midiFileBpm = 120.0;
    for (int t = 0; t < tempMidiFile.getNumTracks(); ++t) {
        auto* track = tempMidiFile.getTrack(t);
        if (track) {
            for (int i = 0; i < track->getNumEvents(); ++i) {
                auto& msg = track->getEventPointer(i)->message;
                if (msg.isTempoMetaEvent()) {
                    double secPerQuarter = msg.getTempoSecondsPerQuarterNote();
                    midiFileBpm = 60.0 / secPerQuarter;
                    break;
                }
            }
        }
    }
    
    tempMidiFile.convertTimestampTicksToSeconds();
    currentMidiFile = tempMidiFile;
    
    playbackSequence.clear();
    for (int t = 0; t < currentMidiFile.getNumTracks(); ++t) {
        auto* track = currentMidiFile.getTrack(t);
        if (track) {
            for (int i = 0; i < track->getNumEvents(); ++i) {
                playbackSequence.addEvent(track->getEventPointer(i)->message);
            }
        }
    }
    playbackSequence.sort();
    
    fileLoaded = true;
    playbackNoteIndex = 0;
    playbackStartTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    playbackStartBeat = getHostBeatPosition();
    transportSlider.setValue(0, juce::dontSendNotification);
}

double MIDIXplorerEditor::getHostBpm() {
    if (auto* playHead = getAudioProcessor()->getPlayHead()) {
        if (auto posInfo = playHead->getPosition()) {
            if (auto bpm = posInfo->getBpm()) {
                return *bpm;
            }
        }
    }
    return 120.0;
}

double MIDIXplorerEditor::getHostBeatPosition() {
    if (auto* playHead = getAudioProcessor()->getPlayHead()) {
        if (auto posInfo = playHead->getPosition()) {
            if (auto ppq = posInfo->getPpqPosition()) {
                return *ppq;
            }
        }
    }
    return 0.0;
}

bool MIDIXplorerEditor::isHostPlaying() {
    if (auto* playHead = getAudioProcessor()->getPlayHead()) {
        if (auto posInfo = playHead->getPosition()) {
            return posInfo->getIsPlaying();
        }
    }
    return false;
}

void MIDIXplorerEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xFFECECEC));
    
    g.setColour(juce::Colour(0xFFE0E0E0));
    g.fillRect(0, 0, 150, getHeight());
    
    g.setColour(juce::Colour(0xFFD8D8D8));
    g.fillRect(0, 0, getWidth(), 35);
    
    g.setColour(juce::Colour(0xFFCCCCCC));
    g.drawLine(150.0f, 0.0f, 150.0f, (float)getHeight(), 1.0f);
    g.drawLine(0.0f, 35.0f, (float)getWidth(), 35.0f, 1.0f);
    
    g.setColour(juce::Colours::darkgrey);
    g.setFont(juce::FontOptions(11.0f));
    juce::String statusText = juce::String(lastHostBpm, 1) + " BPM";
    if (syncToHostToggle.getToggleState()) {
        statusText += isHostPlaying() ? " ▶" : " ⏸";
    }
    g.drawText(statusText, getWidth() - 100, 8, 90, 20, juce::Justification::right);
}

void MIDIXplorerEditor::resized() {
    auto area = getLocalBounds();
    
    auto topBar = area.removeFromTop(35);
    previewToggle.setBounds(topBar.removeFromLeft(80).reduced(4));
    syncToHostToggle.setBounds(topBar.removeFromLeft(60).reduced(4));
    transportSlider.setBounds(topBar.removeFromLeft(120).reduced(4));
    
    auto sidebar = area.removeFromLeft(150);
    addLibraryBtn.setBounds(sidebar.removeFromTop(25).reduced(5, 2));
    
    auto libHeader = sidebar.removeFromTop(20);
    librariesLabel.setBounds(libHeader.reduced(5, 0));
    
    libraryListBox.setBounds(sidebar.reduced(2));
    
    auto header = area.removeFromTop(25);
    fileCountLabel.setBounds(header.removeFromLeft(200).reduced(5, 0));
    keyFilterCombo.setBounds(header.removeFromRight(100).reduced(2));
    searchBox.setBounds(header.removeFromRight(150).reduced(2));
    
    fileListBox->setBounds(area.reduced(2));
}

void MIDIXplorerEditor::timerCallback() {
    lastHostBpm = getHostBpm();
    bool hostPlaying = isHostPlaying();
    double currentBeat = getHostBeatPosition();
    
    // Analyze files progressively
    bool needsKeyUpdate = false;
    for (size_t i = 0; i < allFiles.size(); ++i) {
        if (!allFiles[i].analyzed) {
            analyzeFile(i);
            needsKeyUpdate = true;
            filterFiles();
            break;
        }
    }
    
    if (needsKeyUpdate) {
        updateKeyFilterFromDetectedScales();
    }
    
    // Handle pending file change on beat boundary
    if (pendingFileChange && syncToHostToggle.getToggleState() && hostPlaying) {
        // Check if we crossed a beat boundary
        double currentBeatFrac = currentBeat - std::floor(currentBeat);
        if (currentBeatFrac < 0.1 || lastBeatPosition > currentBeat) {
            // We're at a beat boundary, load the new file
            selectedFileIndex = pendingFileIndex;
            loadSelectedFile();
            pendingFileChange = false;
            playbackStartBeat = std::floor(currentBeat);
        }
    }
    lastBeatPosition = currentBeat;
    
    // Handle playback
    if (!fileLoaded || !previewToggle.getToggleState() || !pluginProcessor) {
        // Trigger repaint for status
        repaint(getWidth() - 110, 0, 110, 35);
        return;
    }
    
    if (syncToHostToggle.getToggleState()) {
        // ===== SYNCED TO HOST =====
        
        // Detect host start
        if (hostPlaying && !wasHostPlaying) {
            // Host just started - reset playback to sync
            playbackStartBeat = currentBeat;
            playbackNoteIndex = 0;
            for (int ch = 1; ch <= 16; ++ch) {
                pluginProcessor->addMidiMessage(juce::MidiMessage::allNotesOff(ch));
            }
        }
        
        // Detect host stop
        if (!hostPlaying && wasHostPlaying) {
            // Host stopped - stop notes
            for (int ch = 1; ch <= 16; ++ch) {
                pluginProcessor->addMidiMessage(juce::MidiMessage::allNotesOff(ch));
            }
        }
        
        wasHostPlaying = hostPlaying;
        
        if (!hostPlaying) {
            repaint(getWidth() - 110, 0, 110, 35);
            return;
        }
        
        // Calculate playback position based on host beats
        double beatsElapsed = currentBeat - playbackStartBeat;
        
        // Handle transport jump backwards
        if (beatsElapsed < -0.5) {
            playbackStartBeat = currentBeat;
            playbackNoteIndex = 0;
            beatsElapsed = 0;
            for (int ch = 1; ch <= 16; ++ch) {
                pluginProcessor->addMidiMessage(juce::MidiMessage::allNotesOff(ch));
            }
        }
        
        // Convert beats to MIDI file time
        // 1 beat = 60/BPM seconds
        double secondsPerMidiBeat = 60.0 / midiFileBpm;
        double midiPlaybackPos = beatsElapsed * secondsPerMidiBeat;
        
        // Play events
        while (playbackNoteIndex < playbackSequence.getNumEvents()) {
            auto* evt = playbackSequence.getEventPointer(playbackNoteIndex);
            if (evt->message.getTimeStamp() <= midiPlaybackPos) {
                pluginProcessor->addMidiMessage(evt->message);
                playbackNoteIndex++;
            } else {
                break;
            }
        }
        
        // Update slider
        double totalTime = playbackSequence.getEndTime();
        if (totalTime > 0) {
            double progress = std::fmod(midiPlaybackPos, totalTime) / totalTime;
            if (progress < 0) progress = 0;
            transportSlider.setValue(progress, juce::dontSendNotification);
        }
        
        // Loop
        if (playbackNoteIndex >= playbackSequence.getNumEvents()) {
            for (int ch = 1; ch <= 16; ++ch) {
                pluginProcessor->addMidiMessage(juce::MidiMessage::allNotesOff(ch));
            }
            playbackStartBeat = currentBeat;
            playbackNoteIndex = 0;
        }
        
    } else {
        // ===== FREE RUNNING =====
        double elapsed = juce::Time::getMillisecondCounterHiRes() / 1000.0 - playbackStartTime;
        
        while (playbackNoteIndex < playbackSequence.getNumEvents()) {
            auto* evt = playbackSequence.getEventPointer(playbackNoteIndex);
            if (evt->message.getTimeStamp() <= elapsed) {
                pluginProcessor->addMidiMessage(evt->message);
                playbackNoteIndex++;
            } else {
                break;
            }
        }
        
        double totalTime = playbackSequence.getEndTime();
        if (totalTime > 0) {
            transportSlider.setValue(std::fmod(elapsed, totalTime) / totalTime, juce::dontSendNotification);
        }
        
        if (playbackNoteIndex >= playbackSequence.getNumEvents()) {
            for (int ch = 1; ch <= 16; ++ch) {
                pluginProcessor->addMidiMessage(juce::MidiMessage::allNotesOff(ch));
            }
            playbackStartTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
            playbackNoteIndex = 0;
        }
    }
    
    repaint(getWidth() - 110, 0, 110, 35);
}

void MIDIXplorerEditor::addLibrary() {
    fileChooser = std::make_unique<juce::FileChooser>("Select MIDI Library Folder",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory), "", true);
    
    auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories;
    
    fileChooser->launchAsync(flags, [this](const juce::FileChooser& fc) {
        auto result = fc.getResult();
        if (result.exists() && result.isDirectory()) {
            Library lib;
            lib.name = result.getFileName();
            lib.path = result.getFullPathName();
            lib.enabled = true;
            libraries.push_back(lib);
            
            libraryListBox.updateContent();
            scanLibrary(libraries.size() - 1);
        }
    });
}

void MIDIXplorerEditor::scanLibraries() {
    allFiles.clear();
    detectedKeys.clear();
    for (size_t i = 0; i < libraries.size(); ++i) {
        scanLibrary(i);
    }
    filterFiles();
    updateKeyFilterFromDetectedScales();
}

void MIDIXplorerEditor::scanLibrary(size_t index) {
    if (index >= libraries.size()) return;
    
    auto& lib = libraries[index];
    lib.isScanning = true;
    lib.fileCount = 0;
    libraryListBox.repaint();
    
    juce::File folder(lib.path);
    if (folder.isDirectory()) {
        auto files = folder.findChildFiles(juce::File::findFiles, true, "*.mid;*.midi");
        for (const auto& file : files) {
            MIDIFileInfo info;
            info.fileName = file.getFileName();
            info.fullPath = file.getFullPathName();
            info.libraryName = lib.name;
            info.analyzed = false;
            allFiles.push_back(info);
            lib.fileCount++;
        }
    }
    
    lib.isScanning = false;
    libraryListBox.repaint();
    filterFiles();
}

void MIDIXplorerEditor::analyzeFile(size_t index) {
    if (index >= allFiles.size()) return;
    
    auto& info = allFiles[index];
    juce::File file(info.fullPath);
    juce::FileInputStream stream(file);
    
    if (stream.openedOk()) {
        juce::MidiFile midiFile;
        if (midiFile.readFrom(stream)) {
            midiFile.convertTimestampTicksToSeconds();
            info.key = detectKey(midiFile);
            
            if (info.key != "?" && !detectedKeys.contains(info.key)) {
                detectedKeys.add(info.key);
            }
        } else {
            info.key = "?";
        }
    } else {
        info.key = "?";
    }
    info.analyzed = true;
}

void MIDIXplorerEditor::filterFiles() {
    filteredFiles.clear();
    
    juce::String searchText = searchBox.getText().toLowerCase();
    juce::String keyFilter = keyFilterCombo.getText();
    bool filterByKey = (keyFilterCombo.getSelectedId() > 1);
    
    for (const auto& file : allFiles) {
        bool libEnabled = true;
        for (const auto& lib : libraries) {
            if (lib.name == file.libraryName) {
                libEnabled = lib.enabled;
                break;
            }
        }
        if (!libEnabled) continue;
        
        if (searchText.isNotEmpty() && !file.fileName.toLowerCase().contains(searchText))
            continue;
        
        if (filterByKey && file.key != keyFilter)
            continue;
        
        filteredFiles.push_back(file);
    }
    
    fileListBox->updateContent();
    fileCountLabel.setText("Name (" + juce::String((int)filteredFiles.size()) + " files)", juce::dontSendNotification);
}

void MIDIXplorerEditor::updateKeyFilterFromDetectedScales() {
    int currentSelection = keyFilterCombo.getSelectedId();
    juce::String currentText = keyFilterCombo.getText();
    
    keyFilterCombo.clear();
    keyFilterCombo.addItem("All Keys", 1);
    
    juce::StringArray majorKeys, minorKeys;
    for (const auto& key : detectedKeys) {
        if (key.contains("Major")) {
            majorKeys.add(key);
        } else if (key.contains("min")) {
            minorKeys.add(key);
        }
    }
    majorKeys.sort(false);
    minorKeys.sort(false);
    
    int id = 2;
    for (const auto& key : majorKeys) {
        keyFilterCombo.addItem(key, id++);
    }
    for (const auto& key : minorKeys) {
        keyFilterCombo.addItem(key, id++);
    }
    
    if (currentSelection > 1) {
        for (int i = 0; i < keyFilterCombo.getNumItems(); ++i) {
            if (keyFilterCombo.getItemText(i) == currentText) {
                keyFilterCombo.setSelectedId(keyFilterCombo.getItemId(i), juce::dontSendNotification);
                return;
            }
        }
    }
    keyFilterCombo.setSelectedId(1, juce::dontSendNotification);
}

void MIDIXplorerEditor::stopPlayback() {
    if (pluginProcessor) {
        for (int ch = 1; ch <= 16; ++ch) {
            pluginProcessor->addMidiMessage(juce::MidiMessage::allNotesOff(ch));
        }
    }
    fileLoaded = false;
    playbackNoteIndex = 0;
    wasHostPlaying = false;
    pendingFileChange = false;
}

void MIDIXplorerEditor::revealInFinder(const juce::String& path) {
    juce::File(path).revealToUser();
}
