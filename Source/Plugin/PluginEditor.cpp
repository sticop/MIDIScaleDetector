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
        owner.playSelectedFile();
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
    
    keyFilterCombo.addItem("Key", 1);
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
            playSelectedFile();
        }
    };
    addAndMakeVisible(previewToggle);
    
    transportSlider.setRange(0, 1, 0.01);
    transportSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    transportSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    addAndMakeVisible(transportSlider);
    
    setWantsKeyboardFocus(true);
    startTimer(20);
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
        playSelectedFile();
    }
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
}

void MIDIXplorerEditor::resized() {
    auto area = getLocalBounds();
    
    auto topBar = area.removeFromTop(35);
    previewToggle.setBounds(topBar.removeFromLeft(80).reduced(4));
    transportSlider.setBounds(topBar.removeFromLeft(150).reduced(4));
    
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
    // Analyze files progressively
    for (size_t i = 0; i < allFiles.size(); ++i) {
        if (!allFiles[i].analyzed) {
            analyzeFile(i);
            filterFiles();
            break;
        }
    }
    
    // Handle playback with looping
    if (isPlaying && pluginProcessor && previewToggle.getToggleState()) {
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
        
        // Update transport slider
        double totalTime = playbackSequence.getEndTime();
        if (totalTime > 0) {
            double progress = std::fmod(elapsed, totalTime) / totalTime;
            transportSlider.setValue(progress, juce::dontSendNotification);
        }
        
        // Loop when finished
        if (playbackNoteIndex >= playbackSequence.getNumEvents()) {
            for (int ch = 1; ch <= 16; ++ch) {
                pluginProcessor->addMidiMessage(juce::MidiMessage::allNotesOff(ch));
            }
            playbackStartTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
            playbackNoteIndex = 0;
        }
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
    if (!pluginProcessor) return;
    
    if (isPlaying) {
        for (int ch = 1; ch <= 16; ++ch) {
            pluginProcessor->addMidiMessage(juce::MidiMessage::allNotesOff(ch));
        }
    }
    
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
    
    isPlaying = true;
    playbackStartTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    playbackNoteIndex = 0;
    transportSlider.setValue(0, juce::dontSendNotification);
}

void MIDIXplorerEditor::stopPlayback() {
    if (isPlaying && pluginProcessor) {
        for (int ch = 1; ch <= 16; ++ch) {
            pluginProcessor->addMidiMessage(juce::MidiMessage::allNotesOff(ch));
        }
    }
    isPlaying = false;
    playbackNoteIndex = 0;
}

void MIDIXplorerEditor::revealInFinder(const juce::String& path) {
    juce::File(path).revealToUser();
}
