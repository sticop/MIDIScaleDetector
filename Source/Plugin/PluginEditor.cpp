#include "PluginEditor.h"

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
    
    // Checkbox
    g.setColour(juce::Colours::darkgrey);
    g.drawRect(4, h/2 - 6, 12, 12);
    if (lib.enabled) {
        g.setColour(juce::Colours::black);
        g.drawLine(6.0f, (float)(h/2), 9.0f, (float)(h/2 + 3), 2.0f);
        g.drawLine(9.0f, (float)(h/2 + 3), 14.0f, (float)(h/2 - 4), 2.0f);
    }
    
    // Name and count
    g.setColour(juce::Colours::black);
    g.drawText(lib.name, 22, 0, w - 70, h, juce::Justification::left);
    
    // File count or scanning indicator
    if (lib.isScanning) {
        g.setColour(juce::Colours::orange);
        g.drawText("...", w - 45, 0, 40, h, juce::Justification::right);
    } else {
        g.setColour(juce::Colours::grey);
        g.drawText(juce::String(lib.fileCount) + " ?", w - 45, 0, 40, h, juce::Justification::right);
    }
}

void MIDIXplorerEditor::LibraryListModel::listBoxItemClicked(int row, const juce::MouseEvent& e) {
    if (row < 0 || row >= (int)owner.libraries.size()) return;
    
    // Check if clicked on checkbox area
    if (e.x < 20) {
        owner.libraries[(size_t)row].enabled = !owner.libraries[(size_t)row].enabled;
        owner.libraryListBox.repaint();
        owner.filterFiles();
    }
    // Right click for context menu
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
    
    // File name
    g.setColour(juce::Colours::black);
    g.drawText(file.fileName, 10, 0, w - 120, h, juce::Justification::left);
    
    // Key
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
    libraryModel = std::make_unique<LibraryListModel>(*this);
    fileModel = std::make_unique<FileListModel>(*this);
    
    // Add Library button
    addLibraryBtn.onClick = [this]() { addLibrary(); };
    addAndMakeVisible(addLibraryBtn);
    
    // Libraries label
    librariesLabel.setText("Libraries", juce::dontSendNotification);
    librariesLabel.setFont(juce::FontOptions(14.0f));
    librariesLabel.setColour(juce::Label::textColourId, juce::Colours::black);
    addAndMakeVisible(librariesLabel);
    
    // Library list
    libraryListBox.setModel(libraryModel.get());
    libraryListBox.setRowHeight(22);
    libraryListBox.setColour(juce::ListBox::backgroundColourId, juce::Colours::white);
    addAndMakeVisible(libraryListBox);
    
    // File count label
    fileCountLabel.setText("Name (0 files / 0 files shown)", juce::dontSendNotification);
    fileCountLabel.setColour(juce::Label::textColourId, juce::Colours::black);
    addAndMakeVisible(fileCountLabel);
    
    // Key filter
    keyFilterCombo.addItem("Key", 1);
    keyFilterCombo.setSelectedId(1);
    keyFilterCombo.onChange = [this]() { filterFiles(); };
    addAndMakeVisible(keyFilterCombo);
    
    // Search box
    searchBox.setTextToShowWhenEmpty("Search Midi Files", juce::Colours::grey);
    searchBox.onTextChange = [this]() { filterFiles(); };
    addAndMakeVisible(searchBox);
    
    // File list
    fileListBox.setModel(fileModel.get());
    fileListBox.setRowHeight(20);
    fileListBox.setColour(juce::ListBox::backgroundColourId, juce::Colours::white);
    addAndMakeVisible(fileListBox);
    
    // Transport
    playBtn.setButtonText(">");
    playBtn.onClick = [this]() { playSelectedFile(); };
    addAndMakeVisible(playBtn);
    
    stopBtn.setButtonText("[]");
    stopBtn.onClick = [this]() { stopPlayback(); };
    addAndMakeVisible(stopBtn);
    
    transportSlider.setRange(0, 1, 0.01);
    transportSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    transportSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    addAndMakeVisible(transportSlider);
    
    // Try MIDI output
    auto devices = juce::MidiOutput::getAvailableDevices();
    if (!devices.isEmpty()) {
        midiOutput = juce::MidiOutput::openDevice(devices[0].identifier);
    }
    
    startTimer(50);
    setSize(900, 550);
}

MIDIXplorerEditor::~MIDIXplorerEditor() {
    stopTimer();
    stopPlayback();
}

void MIDIXplorerEditor::paint(juce::Graphics& g) {
    // Light gray background like the reference
    g.fillAll(juce::Colour(0xFFECECEC));
    
    // Left panel background
    g.setColour(juce::Colour(0xFFE0E0E0));
    g.fillRect(0, 0, 150, getHeight());
    
    // Top bar
    g.setColour(juce::Colour(0xFFD8D8D8));
    g.fillRect(0, 0, getWidth(), 35);
    
    // Separator lines
    g.setColour(juce::Colour(0xFFCCCCCC));
    g.drawLine(150.0f, 0.0f, 150.0f, (float)getHeight(), 1.0f);
    g.drawLine(0.0f, 35.0f, (float)getWidth(), 35.0f, 1.0f);
}

void MIDIXplorerEditor::resized() {
    auto area = getLocalBounds();
    
    // Top transport bar
    auto topBar = area.removeFromTop(35);
    playBtn.setBounds(topBar.removeFromLeft(30).reduced(4));
    stopBtn.setBounds(topBar.removeFromLeft(30).reduced(4));
    transportSlider.setBounds(topBar.removeFromLeft(120).reduced(4));
    
    // Left sidebar
    auto sidebar = area.removeFromLeft(150);
    addLibraryBtn.setBounds(sidebar.removeFromTop(25).reduced(5, 2));
    
    auto libHeader = sidebar.removeFromTop(20);
    librariesLabel.setBounds(libHeader.reduced(5, 0));
    
    libraryListBox.setBounds(sidebar.reduced(2));
    
    // Main content area
    auto header = area.removeFromTop(25);
    fileCountLabel.setBounds(header.removeFromLeft(250).reduced(5, 0));
    keyFilterCombo.setBounds(header.removeFromRight(100).reduced(2));
    searchBox.setBounds(header.removeFromRight(150).reduced(2));
    
    fileListBox.setBounds(area.reduced(2));
}

void MIDIXplorerEditor::timerCallback() {
    // Analyze files progressively
    for (size_t i = 0; i < allFiles.size(); ++i) {
        if (!allFiles[i].analyzed) {
            analyzeFile(i);
            filterFiles();
            break;
        }
    }
    
    // Handle playback
    if (isPlaying && midiOutput) {
        double elapsed = juce::Time::getMillisecondCounterHiRes() / 1000.0 - playbackStartTime;
        while (playbackNoteIndex < playbackSequence.getNumEvents()) {
            auto* evt = playbackSequence.getEventPointer(playbackNoteIndex);
            if (evt->message.getTimeStamp() <= elapsed) {
                midiOutput->sendMessageNow(evt->message);
                playbackNoteIndex++;
            } else break;
        }
        if (playbackNoteIndex >= playbackSequence.getNumEvents()) stopPlayback();
    }
}

void MIDIXplorerEditor::addLibrary() {
    fileChooser = std::make_unique<juce::FileChooser>("Select MIDI Library Folder",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory), "", true);
    
    auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories;
    
    fileChooser->launchAsync(flags, [this](const juce::FileChooser& fc) {
        auto result = fc.getResult();
        if (result.exists() && result.isDirectory()) {
            Library lib;
            lib.name = "Library " + juce::String((int)libraries.size() + 1);
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
    for (size_t i = 0; i < libraries.size(); ++i) {
        scanLibrary(i);
    }
    filterFiles();
    populateKeyFilter();
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
        // Check if library is enabled
        bool libEnabled = true;
        for (const auto& lib : libraries) {
            if (lib.name == file.libraryName) {
                libEnabled = lib.enabled;
                break;
            }
        }
        if (!libEnabled) continue;
        
        // Search filter
        if (searchText.isNotEmpty() && !file.fileName.toLowerCase().contains(searchText))
            continue;
        
        // Key filter
        if (filterByKey && file.key != keyFilter)
            continue;
        
        filteredFiles.push_back(file);
    }
    
    fileListBox.updateContent();
    fileCountLabel.setText("Name (" + juce::String((int)filteredFiles.size()) + " files / " + 
                           juce::String((int)allFiles.size()) + " files shown)", juce::dontSendNotification);
}

void MIDIXplorerEditor::populateKeyFilter() {
    keyFilterCombo.clear();
    keyFilterCombo.addItem("Key", 1);
    
    juce::StringArray keys;
    for (const auto& file : allFiles) {
        if (file.analyzed && file.key != "?" && !keys.contains(file.key)) {
            keys.add(file.key);
        }
    }
    keys.sort(false);
    
    int id = 2;
    for (const auto& key : keys) {
        keyFilterCombo.addItem(key, id++);
    }
    
    keyFilterCombo.setSelectedId(1);
}

void MIDIXplorerEditor::playSelectedFile() {
    if (selectedFileIndex < 0 || selectedFileIndex >= (int)filteredFiles.size()) return;
    
    stopPlayback();
    
    juce::File file(filteredFiles[(size_t)selectedFileIndex].fullPath);
    juce::FileInputStream stream(file);
    
    if (!stream.openedOk() || !currentMidiFile.readFrom(stream)) return;
    
    currentMidiFile.convertTimestampTicksToSeconds();
    
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
    
    if (!midiOutput) {
        auto devices = juce::MidiOutput::getAvailableDevices();
        if (!devices.isEmpty()) midiOutput = juce::MidiOutput::openDevice(devices[0].identifier);
    }
    
    if (midiOutput) {
        isPlaying = true;
        playbackStartTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        playbackNoteIndex = 0;
    }
}

void MIDIXplorerEditor::stopPlayback() {
    if (isPlaying && midiOutput) {
        for (int ch = 1; ch <= 16; ++ch)
            midiOutput->sendMessageNow(juce::MidiMessage::allNotesOff(ch));
    }
    isPlaying = false;
    playbackNoteIndex = 0;
}

void MIDIXplorerEditor::revealInFinder(const juce::String& path) {
    juce::File(path).revealToUser();
}
