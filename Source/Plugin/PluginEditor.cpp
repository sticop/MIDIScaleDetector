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

    // Quantize dropdown (Off = no quantize, selecting a value applies it)
    quantizeCombo.addItem("Quantize: Off", 100);
    quantizeCombo.addSectionHeading("Straight Notes");
    quantizeCombo.addItem("1/1 Note", 1);
    quantizeCombo.addItem("1/2 Note", 2);
    quantizeCombo.addItem("1/4 Note", 3);
    quantizeCombo.addItem("1/8 Note", 4);
    quantizeCombo.addItem("1/16 Note", 5);
    quantizeCombo.addItem("1/32 Note", 6);
    quantizeCombo.addItem("1/64 Note", 7);
    quantizeCombo.addSectionHeading("Triplets");
    quantizeCombo.addItem("1/2 Triplet (1/3)", 8);
    quantizeCombo.addItem("1/4 Triplet (1/6)", 9);
    quantizeCombo.addItem("1/8 Triplet (1/12)", 10);
    quantizeCombo.addItem("1/16 Triplet (1/24)", 11);
    quantizeCombo.addItem("1/32 Triplet (1/48)", 12);
    quantizeCombo.addItem("1/64 Triplet (1/96)", 13);
    quantizeCombo.addItem("1/128 Triplet (1/192)", 14);
    quantizeCombo.addSectionHeading("1/16 Swing");
    quantizeCombo.addItem("1/16 Swing A", 15);
    quantizeCombo.addItem("1/16 Swing B", 16);
    quantizeCombo.addItem("1/16 Swing C", 17);
    quantizeCombo.addItem("1/16 Swing D", 18);
    quantizeCombo.addItem("1/16 Swing E", 19);
    quantizeCombo.addItem("1/16 Swing F", 20);
    quantizeCombo.addSectionHeading("1/8 Swing");
    quantizeCombo.addItem("1/8 Swing A", 21);
    quantizeCombo.addItem("1/8 Swing B", 22);
    quantizeCombo.addItem("1/8 Swing C", 23);
    quantizeCombo.addItem("1/8 Swing D", 24);
    quantizeCombo.addItem("1/8 Swing E", 25);
    quantizeCombo.addItem("1/8 Swing F", 26);
    quantizeCombo.addSectionHeading("Tuplets");
    quantizeCombo.addItem("5-Tuplet/4", 27);
    quantizeCombo.addItem("5-Tuplet/8", 28);
    quantizeCombo.addItem("7-Tuplet", 29);
    quantizeCombo.addItem("9-Tuplet", 30);
    quantizeCombo.addSectionHeading("Mixed");
    quantizeCombo.addItem("1/16 & 1/16 Triplet", 31);
    quantizeCombo.addItem("1/16 & 1/8 Triplet", 32);
    quantizeCombo.addItem("1/8 & 1/8 Triplet", 33);
    quantizeCombo.setSelectedId(100);  // Default to Off
    quantizeCombo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff3a3a3a));
    quantizeCombo.onChange = [this]() {
        if (quantizeCombo.getSelectedId() != 100) {
            quantizeMidi();
        } else if (isQuantized) {
            // When switching back to Off, reload original file
            loadSelectedFile();
        }
    };
    addAndMakeVisible(quantizeCombo);

    // Search box with modern rounded style
    searchBox.setTextToShowWhenEmpty("Search files or keys (C Major, Am)...", juce::Colours::grey);
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
                // Start fresh playback
                playbackNoteIndex = 0;
                playbackStartTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
                playbackStartBeat = getHostBeatPosition();
            }
            // Set processor playback state
            if (pluginProcessor) {
                pluginProcessor->setPlaybackPlaying(true);
                pluginProcessor->resetPlayback();
            }
        } else {
            // Pause/Stop
            isPlaying = false;
            playPauseButton.setButtonText(juce::String::fromUTF8("\u25B6"));  // Play icon
            playbackNoteIndex = 0;  // Reset for next play
            // Send note-offs for active notes when stopping
            if (pluginProcessor) {
                pluginProcessor->setPlaybackPlaying(false);
                pluginProcessor->sendActiveNoteOffs();
            }
        }
    };
    addAndMakeVisible(playPauseButton);

    // Drag to DAW button
    dragButton.setButtonText(juce::String::fromUTF8("\u2B07"));  // Down arrow icon
    dragButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3a3a3a));
    dragButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff5a5a5a));
    dragButton.setColour(juce::TextButton::textColourOffId, juce::Colours::orange);
    dragButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    dragButton.setTooltip("Drag selected MIDI file to DAW");
    dragButton.onClick = [this]() {
        // Start drag operation for selected file
        int selectedRow = fileListBox->getLastRowSelected();
        if (selectedRow >= 0 && selectedRow < static_cast<int>(filteredFiles.size())) {
            auto& selectedFile = filteredFiles[static_cast<size_t>(selectedRow)];
            juce::StringArray files;
            files.add(selectedFile.fullPath);
            juce::DragAndDropContainer::performExternalDragDropOfFiles(files, true);
        }
    };
    addAndMakeVisible(dragButton);

    // Add to DAW button
    addToDAWButton.setButtonText("Add to DAW");
    addToDAWButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3a5a3a));
    addToDAWButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff4a7a4a));
    addToDAWButton.setColour(juce::TextButton::textColourOffId, juce::Colours::lightgreen);
    addToDAWButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    addToDAWButton.setTooltip("Add MIDI at DAW playhead position");
    addToDAWButton.onClick = [this]() {
        // Get the selected MIDI file and send it to DAW at playhead
        int selectedRow = fileListBox->getLastRowSelected();
        if (selectedRow >= 0 && selectedRow < static_cast<int>(filteredFiles.size())) {
            auto& selectedFile = filteredFiles[static_cast<size_t>(selectedRow)];
            if (pluginProcessor) {
                // Load and queue MIDI data to be inserted at playhead
                juce::File midiFile(selectedFile.fullPath);
                if (midiFile.existsAsFile()) {
                    juce::MidiFile midi;
                    juce::FileInputStream fis(midiFile);
                    if (fis.openedOk() && midi.readFrom(fis)) {
                        // Queue MIDI for insertion at current playhead
                        pluginProcessor->queueMidiForInsertion(midi);
                    }
                }
            }
        }
    };
    // addToDAWButton hidden - drag button is preferred
    addChildComponent(addToDAWButton);

    // Zoom is now handled by mouse wheel in the MIDI viewer

    // Transpose dropdown
    transposeComboBox.addItem("+36", 1);
    transposeComboBox.addItem("+24", 2);
    transposeComboBox.addItem("+12", 3);
    transposeComboBox.addItem(juce::CharPointer_UTF8("\xc2\xb1 0"), 4);  // ± 0
    transposeComboBox.addItem("-12", 5);
    transposeComboBox.addItem("-24", 6);
    transposeComboBox.addItem("-36", 7);
    transposeComboBox.setSelectedId(4);  // Default to ± 0
    transposeComboBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff3a3a3a));
    transposeComboBox.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    transposeComboBox.setColour(juce::ComboBox::arrowColourId, juce::Colours::white);
    transposeComboBox.setTooltip("Transpose");
    transposeComboBox.onChange = [this]() {
        int selectedId = transposeComboBox.getSelectedId();
        switch (selectedId) {
            case 1: transposeAmount = 36; break;
            case 2: transposeAmount = 24; break;
            case 3: transposeAmount = 12; break;
            case 4: transposeAmount = 0; break;
            case 5: transposeAmount = -12; break;
            case 6: transposeAmount = -24; break;
            case 7: transposeAmount = -36; break;
            default: transposeAmount = 0; break;
        }
        applyTransposeToPlayback();
    };
    addAndMakeVisible(transposeComboBox);

    syncToHostToggle.setToggleState(true, juce::dontSendNotification);
    syncToHostToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    syncToHostToggle.setColour(juce::ToggleButton::tickColourId, juce::Colours::orange);
    syncToHostToggle.onClick = [this]() {
        if (pluginProcessor) {
            pluginProcessor->setSyncToHost(syncToHostToggle.getToggleState());
        }
    };
    // syncToHostToggle hidden but functionality preserved
    addChildComponent(syncToHostToggle);

    // Velocity slider for volume control
    velocityLabel.setText("Vol:", juce::dontSendNotification);
    velocityLabel.setFont(juce::Font(12.0f));
    velocityLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    velocityLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(velocityLabel);

    velocitySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    velocitySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
    velocitySlider.setRange(0.0, 200.0, 1.0);  // 0% to 200%
    velocitySlider.setValue(100.0);  // Default 100%
    velocitySlider.setTextValueSuffix("%");
    velocitySlider.setColour(juce::Slider::trackColourId, juce::Colour(0xff3a3a3a));
    velocitySlider.setColour(juce::Slider::thumbColourId, juce::Colours::orange);
    velocitySlider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    velocitySlider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff2a2a2a));
    velocitySlider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff444444));
    velocitySlider.onValueChange = [this]() {
        if (pluginProcessor) {
            pluginProcessor->setVelocityScale((float)(velocitySlider.getValue() / 100.0));
        }
    };
    addAndMakeVisible(velocitySlider);

    transportSlider.setSliderStyle(juce::Slider::LinearBar);
    transportSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    transportSlider.setRange(0.0, 1.0);
    transportSlider.setVelocityBasedMode(false);
    transportSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xff3a3a3a));
    transportSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff00aaff));
    // Transport slider hidden - use MIDI viewer for position
    addChildComponent(transportSlider);


    timeDisplayLabel.setFont(juce::Font(14.0f));
    timeDisplayLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    timeDisplayLabel.setJustificationType(juce::Justification::centredRight);
    timeDisplayLabel.setText("0:00 / 0:00", juce::dontSendNotification);
    addAndMakeVisible(timeDisplayLabel);
    addAndMakeVisible(midiNoteViewer);
    setSize(1200, 750);

    // Load saved libraries
    loadLibraries();
    loadFavorites();
    loadRecentlyPlayed();

    // Restore playback state from processor (in case editor was reopened while playing)
    if (pluginProcessor) {
        isPlaying = pluginProcessor->isPlaybackPlaying();
        if (isPlaying) {
            playPauseButton.setButtonText(juce::String::fromUTF8("\u23F8"));  // Pause icon
        }
        syncToHostToggle.setToggleState(pluginProcessor->isSyncToHost(), juce::dontSendNotification);

        // Restore the selected file path from processor
        juce::String lastPath = pluginProcessor->getCurrentFilePath();
        if (lastPath.isNotEmpty()) {
            pendingSelectedFilePath = lastPath;
        }
    }

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

juce::File MIDIXplorerEditor::getCacheFile() {
    auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    auto pluginDir = appDataDir.getChildFile("MIDIXplorer");
    if (!pluginDir.exists()) {
        pluginDir.createDirectory();
    }
    return pluginDir.getChildFile("filecache.json");
}

void MIDIXplorerEditor::saveFileCache() {
    auto file = getCacheFile();
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    juce::Array<juce::var> filesArray;

    for (const auto& f : allFiles) {
        if (f.analyzed) {  // Only cache analyzed files
            juce::DynamicObject::Ptr fileObj = new juce::DynamicObject();
            fileObj->setProperty("fullPath", f.fullPath);
            fileObj->setProperty("fileName", f.fileName);
            fileObj->setProperty("libraryName", f.libraryName);
            fileObj->setProperty("key", f.key);
            fileObj->setProperty("relativeKey", f.relativeKey);
            fileObj->setProperty("duration", f.duration);
            fileObj->setProperty("durationBeats", f.durationBeats);
            fileObj->setProperty("bpm", f.bpm);
            fileObj->setProperty("fileSize", (juce::int64)f.fileSize);
            fileObj->setProperty("instrument", f.instrument);
            fileObj->setProperty("analyzed", f.analyzed);
            filesArray.add(juce::var(fileObj.get()));
        }
    }
    root->setProperty("files", filesArray);
    root->setProperty("cacheVersion", 1);

    juce::var jsonVar(root.get());
    auto jsonStr = juce::JSON::toString(jsonVar);
    file.replaceWithText(jsonStr);
}

void MIDIXplorerEditor::loadFileCache() {
    auto file = getCacheFile();
    if (!file.existsAsFile()) return;

    auto jsonStr = file.loadFileAsString();
    auto jsonVar = juce::JSON::parse(jsonStr);

    if (auto* obj = jsonVar.getDynamicObject()) {
        auto filesVar = obj->getProperty("files");
        if (auto* filesArray = filesVar.getArray()) {
            for (const auto& fileVar : *filesArray) {
                if (auto* fileObj = fileVar.getDynamicObject()) {
                    MIDIFileInfo info;
                    info.fullPath = fileObj->getProperty("fullPath").toString();
                    info.fileName = fileObj->getProperty("fileName").toString();
                    info.libraryName = fileObj->getProperty("libraryName").toString();
                    info.key = fileObj->getProperty("key").toString();
                    info.relativeKey = fileObj->getProperty("relativeKey").toString();
                    info.duration = (double)fileObj->getProperty("duration");
                    info.durationBeats = (double)fileObj->getProperty("durationBeats");
                    info.bpm = (double)fileObj->getProperty("bpm");
                    info.fileSize = (juce::int64)fileObj->getProperty("fileSize");
                    info.instrument = fileObj->getProperty("instrument").toString();
                    info.analyzed = (bool)fileObj->getProperty("analyzed");

                    // Only add if file still exists
                    if (juce::File(info.fullPath).existsAsFile()) {
                        allFiles.push_back(info);
                    }
                }
            }
        }
    }
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

    // Save selected file path
    if (selectedFileIndex >= 0 && selectedFileIndex < (int)filteredFiles.size()) {
        root->setProperty("selectedFilePath", filteredFiles[(size_t)selectedFileIndex].fullPath);
    }

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

            // Load cached files first
            loadFileCache();

            // Update library file counts from cache
            for (auto& lib : libraries) {
                lib.fileCount = 0;
                for (const auto& f : allFiles) {
                    if (f.libraryName == lib.name) {
                        lib.fileCount++;
                    }
                }
            }

            // Then scan for any new files
            scanLibraries();
        }

        // Restore selected file
        auto savedPath = obj->getProperty("selectedFilePath").toString();
        if (savedPath.isNotEmpty()) {
            pendingSelectedFilePath = savedPath;
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
    velocityLabel.setBounds(topBar.removeFromLeft(28));
    velocitySlider.setBounds(topBar.removeFromLeft(100).reduced(2));
    // syncToHostToggle hidden

    // Bottom transport bar
    auto transport = area.removeFromBottom(40).reduced(8, 4);
    playPauseButton.setBounds(transport.removeFromLeft(40));
    transport.removeFromLeft(4);
    dragButton.setBounds(transport.removeFromLeft(40));
    // addToDAWButton hidden

    // Quantize and Transpose dropdowns on the right
    transposeComboBox.setBounds(transport.removeFromRight(100));
    transport.removeFromRight(4);
    quantizeCombo.setBounds(transport.removeFromRight(145));
    transport.removeFromRight(8);

    // MIDI Note Viewer - full width
    auto noteViewerArea = area.removeFromBottom(240);
    auto noteViewerBounds = noteViewerArea.reduced(4, 4);
    midiNoteViewer.setBounds(noteViewerBounds);

    // File list fills the rest
    fileListBox->setBounds(area.reduced(4));
}

void MIDIXplorerEditor::timerCallback() {
    // Increment spinner frame for loading animations
    spinnerFrame = (spinnerFrame + 1) % 8;

    // Process background file scanning (non-blocking, incremental)
    if (isScanningFiles && currentDirIterator) {
        int filesFound = 0;
        static constexpr int FILES_PER_SCAN_TICK = 50;  // Discover 50 files per tick

        while (filesFound < FILES_PER_SCAN_TICK && currentDirIterator->next()) {
            auto file = currentDirIterator->getFile();
            auto ext = file.getFileExtension().toLowerCase();
            if (ext == ".mid" || ext == ".midi") {
                // Check for duplicates
                bool isDuplicate = false;
                for (const auto& existingFile : allFiles) {
                    if (existingFile.fullPath == file.getFullPathName()) {
                        isDuplicate = true;
                        break;
                    }
                }
                if (!isDuplicate) {
                    MIDIFileInfo info;
                    info.fileName = file.getFileName();
                    info.fullPath = file.getFullPathName();
                    info.libraryName = libraries[currentScanLibraryIndex].name;
                    info.key = "---";
                    allFiles.push_back(info);
                    libraries[currentScanLibraryIndex].fileCount++;

                    // Queue for analysis
                    analysisQueue.push_back(allFiles.size() - 1);
                    filesFound++;
                }
            }
        }

        // Check if current directory scan is complete
        if (!currentDirIterator->next()) {
            currentDirIterator.reset();
            libraries[currentScanLibraryIndex].isScanning = false;

            // Move to next library in queue
            if (!scanQueue.empty()) {
                currentScanLibraryIndex = scanQueue.front();
                scanQueue.erase(scanQueue.begin());
                libraries[currentScanLibraryIndex].isScanning = true;
                juce::File dir(libraries[currentScanLibraryIndex].path);
                if (dir.isDirectory()) {
                    currentDirIterator = std::make_unique<juce::DirectoryIterator>(dir, true, "*.mid;*.midi");
                }
            } else {
                isScanningFiles = false;
                sortFiles();
            }
            filterFiles();
            libraryListBox.repaint();
        } else {
            // Re-position iterator (we moved past one file checking if done)
            // Update UI periodically during scanning
            if (filesFound > 0) {
                filterFiles();
                libraryListBox.repaint();
            }
        }
    }

    // Process background analysis queue (non-blocking, a few files per tick)
    if (!analysisQueue.empty()) {
        int filesAnalyzed = 0;
        while (!analysisQueue.empty() && filesAnalyzed < FILES_PER_TICK) {
            size_t idx = analysisQueue.front();
            analysisQueue.erase(analysisQueue.begin());
            if (idx < allFiles.size() && !allFiles[idx].analyzed) {
                allFiles[idx].isAnalyzing = true;
                analyzeFile(idx);
                allFiles[idx].isAnalyzing = false;
                filesAnalyzed++;
            }
        }
        // Update UI if we analyzed files
        if (filesAnalyzed > 0) {
            updateKeyFilterFromDetectedScales();
            fileListBox->repaint();
            libraryListBox.repaint();
        }

        // Save cache when analysis completes and no more scanning
        if (analysisQueue.empty() && !isScanningFiles) {
            saveFileCache();
        }
    }

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

            // Start processor playback and reset note index
            if (pluginProcessor) {
                pluginProcessor->clearMidiQueue();
                pluginProcessor->resetPlayback();
                pluginProcessor->setPlaybackPlaying(true);
            }
        } else if (!hostPlaying && wasHostPlaying) {
            // Host stopped - pause playback and update button
            if (pluginProcessor) {
                pluginProcessor->sendActiveNoteOffs();
                pluginProcessor->setPlaybackPlaying(false);
            }
            isPlaying = false;
            playPauseButton.setButtonText(juce::String::fromUTF8("\u25B6"));  // Play icon
            playbackNoteIndex = 0;
        }

        // Detect DAW loop: if host beat jumped backwards significantly, DAW looped
        if (hostPlaying && wasHostPlaying && lastBeatPosition > 0) {
            double beatDiff = hostBeat - lastBeatPosition;
            // If beat jumped backwards by more than 1 beat, DAW probably looped
            if (beatDiff < -0.5) {
                // DAW looped back - restart MIDI aligned to exact playhead position
                if (pluginProcessor) {
                    // Use proper note-offs instead of allNotesOff to prevent cutting chords
                    pluginProcessor->sendActiveNoteOffs();
                }

                // Calculate MIDI file duration in beats for proper loop alignment
                double beatsPerLoop = (midiFileDuration * midiFileBpm) / 60.0;
                if (beatsPerLoop <= 0) beatsPerLoop = 4.0;  // Safety fallback
                
                // Calculate where in the MIDI file we should be based on host position
                double beatsIntoLoop = std::fmod(hostBeat, beatsPerLoop);
                if (beatsIntoLoop < 0) beatsIntoLoop += beatsPerLoop;
                
                // Convert to time offset in MIDI file
                double timeOffsetInFile = (beatsIntoLoop * 60.0) / midiFileBpm;
                
                // Find the note index that corresponds to this time
                playbackNoteIndex = 0;
                
                // Set start beat to align with current host position
                playbackStartBeat = hostBeat - beatsIntoLoop;
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

        // Check if we crossed a beat boundary (or are close to a beat start)
        // Use a wider window to ensure we don't miss the beat
        bool nearBeatStart = currentBeatFrac < 0.2 || currentBeatFrac > 0.95;
        bool beatChanged = (int)std::floor(lastBeatPosition) != currentBeat;
        
        if (nearBeatStart || beatChanged) {
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
    // Only actually sync when host is playing - otherwise use free-run mode
    bool actuallySync = synced && hostPlaying;

    if (actuallySync) {
        // Calculate position based on host beat position relative to when we started
        double beatsElapsed = hostBeat - playbackStartBeat;
        // Convert beats to MIDI file time (seconds at MIDI file's tempo)
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
        // Send note-offs for currently active notes
        if (pluginProcessor) {
            pluginProcessor->sendActiveNoteOffs();
        }
        // Calculate how much we overshot and preserve it for smooth loop
        double overshoot = std::fmod(currentTime, totalDuration);
        playbackNoteIndex = 0;

        if (actuallySync) {
            // When synced, recalculate start beat based on current host position
            // This ensures we stay locked to the host even after multiple loops
            double beatsForOvershoot = (overshoot * midiFileBpm) / 60.0;
            playbackStartBeat = hostBeat - beatsForOvershoot;
        } else {
            playbackStartTime = juce::Time::getMillisecondCounterHiRes() / 1000.0 - overshoot;
        }

        // Update currentTime to the wrapped value for note playback
        currentTime = overshoot;

        // Note index tracking is now handled by the processor
    }

    // Note: Actual note playback is handled by the processor's updatePlayback()
    // The editor only handles UI updates (transport slider, time display, etc.)
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

    // Add to recently played
    addToRecentlyPlayed(filteredFiles[(size_t)row].fullPath);

    // Resume playback when selecting a file
    isPlaying = true;
    playPauseButton.setButtonText(juce::String::fromUTF8("\u23F8"));  // Pause icon

    // Send note-offs for any currently playing notes before switching files
    if (pluginProcessor) {
        pluginProcessor->sendActiveNoteOffs();
    }

    // Always load immediately for responsive browsing
    // The file will sync to DAW tempo if sync is enabled
    loadSelectedFile();
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
    isQuantized = false;  // Reset quantize when loading new file
    quantizeCombo.setSelectedId(100, juce::dontSendNotification);  // Reset to "Off"
    if (selectedFileIndex < 0 || selectedFileIndex >= (int)filteredFiles.size()) return;

    auto& info = filteredFiles[(size_t)selectedFileIndex];
    juce::File file(info.fullPath);

    if (!file.existsAsFile()) return;

    // Note: allNotesOff is handled by selectAndPreview before calling this

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
    double maxEventTime = 0.0;
    for (int i = 0; i < playbackSequence.getNumEvents(); i++) {
        auto* event = playbackSequence.getEventPointer(i);
        double eventTime = event->message.getTimeStamp();
        if (eventTime > maxEventTime) {
            maxEventTime = eventTime;
        }
        // For note-on events, check if there is a matched note-off
        if (event->message.isNoteOn() && event->noteOffObject != nullptr) {
            double noteOffTime = event->noteOffObject->message.getTimeStamp();
            if (noteOffTime > maxEventTime) {
                maxEventTime = noteOffTime;
            }
        }
    }

    // Round duration to nearest bar (4 beats) for clean looping that matches DAW
    double beatsPerSecond = midiFileBpm / 60.0;
    double totalBeats = maxEventTime * beatsPerSecond;
    double bars = std::ceil(totalBeats / 4.0);  // Round up to nearest bar
    if (bars < 1.0) bars = 1.0;
    midiFileDurationBeats = bars * 4.0;
    midiFileDuration = midiFileDurationBeats / beatsPerSecond;

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
        // Load sequence into processor so playback continues when editor is closed
        pluginProcessor->loadPlaybackSequence(playbackSequence, midiFileDuration, midiFileBpm, info.fullPath);
        pluginProcessor->resetPlayback();
        // Start playback in processor (will use free-run if DAW is stopped)
        pluginProcessor->setPlaybackPlaying(true);
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
            pluginProcessor->sendActiveNoteOffs();
        }
        playbackNoteIndex = 0;
        playbackStartTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        playbackStartBeat = getHostBeatPosition();
    }
}

void MIDIXplorerEditor::playNextFile() {
    if (filteredFiles.empty()) return;

    // Sequential next
    int nextIndex = (selectedFileIndex + 1) % (int)filteredFiles.size();

    selectAndPreview(nextIndex);
}

void MIDIXplorerEditor::applyTransposeToPlayback() {
    if (pluginProcessor) {
        pluginProcessor->setTransposeAmount(transposeAmount);
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
    // Don't clear allFiles if we have cached data - just scan for new files
    // allFiles.clear();  // Removed - keep cached files
    analysisQueue.clear();
    scanQueue.clear();

    // If we have cached files, just update the UI without rescanning
    if (!allFiles.empty()) {
        // Queue unanalyzed files for analysis
        for (size_t i = 0; i < allFiles.size(); i++) {
            if (!allFiles[i].analyzed) {
                analysisQueue.push_back(i);
            }
        }
        filterFiles();
        updateKeyFilterFromDetectedScales();
        libraryListBox.repaint();
        return;  // Skip scanning - we have cached data
    }

    // Queue all enabled libraries for background scanning (only if no cache)
    for (size_t i = 0; i < libraries.size(); i++) {
        if (libraries[i].enabled) {
            // Don't reset file count - it was set from cache
            scanQueue.push_back(i);
        }
    }

    // Start scanning first library
    if (!scanQueue.empty()) {
        currentScanLibraryIndex = scanQueue.front();
        scanQueue.erase(scanQueue.begin());
        libraries[currentScanLibraryIndex].isScanning = true;
        juce::File dir(libraries[currentScanLibraryIndex].path);
        if (dir.isDirectory()) {
            currentDirIterator = std::make_unique<juce::DirectoryIterator>(dir, true, "*.mid;*.midi");
            isScanningFiles = true;
        }
    }

    filterFiles();
    updateKeyFilterFromDetectedScales();
    libraryListBox.repaint();
}

void MIDIXplorerEditor::scanLibrary(size_t index) {
    if (index >= libraries.size()) return;

    auto& lib = libraries[index];
    lib.isScanning = true;
    lib.fileCount = 0;

    juce::File dir(lib.path);
    if (!dir.isDirectory()) {
        lib.isScanning = false;
        return;
    }

    // Add to scan queue for background processing
    // Check if already scanning or in queue
    if (isScanningFiles && currentScanLibraryIndex == index) return;
    for (size_t idx : scanQueue) {
        if (idx == index) return;
    }

    if (!isScanningFiles) {
        // Start scanning immediately
        currentScanLibraryIndex = index;
        currentDirIterator = std::make_unique<juce::DirectoryIterator>(dir, true, "*.mid;*.midi");
        isScanningFiles = true;
    } else {
        // Queue for later
        scanQueue.push_back(index);
    }

    libraryListBox.repaint();

    // Ensure the currently playing file stays selected
    if (!filteredFiles.empty()) {
        int indexToSelect = -1;

        // First priority: Keep the currently playing file from processor
        juce::String currentProcessorPath = pluginProcessor ? pluginProcessor->getCurrentFilePath() : juce::String();
        if (currentProcessorPath.isNotEmpty()) {
            for (size_t i = 0; i < filteredFiles.size(); i++) {
                if (filteredFiles[i].fullPath == currentProcessorPath) {
                    indexToSelect = (int)i;
                    break;
                }
            }
        }

        // Second priority: Restore from pending path (e.g., from saved state)
        if (indexToSelect < 0 && pendingSelectedFilePath.isNotEmpty()) {
            for (size_t i = 0; i < filteredFiles.size(); i++) {
                if (filteredFiles[i].fullPath == pendingSelectedFilePath) {
                    indexToSelect = (int)i;
                    break;
                }
            }
            pendingSelectedFilePath = "";  // Clear after using
        }

        // Third priority: Keep current selection if the file path matches
        if (indexToSelect < 0 && selectedFileIndex >= 0 && selectedFileIndex < (int)allFiles.size()) {
            juce::String currentPath = allFiles[selectedFileIndex].fullPath;
            for (size_t i = 0; i < filteredFiles.size(); i++) {
                if (filteredFiles[i].fullPath == currentPath) {
                    indexToSelect = (int)i;
                    break;
                }
            }
        }

        // Last resort: select first file only if nothing is loaded
        if (indexToSelect < 0) {
            indexToSelect = 0;
        }

        fileListBox->selectRow(indexToSelect);
        selectedFileIndex = indexToSelect;

        // Only load a new file if nothing is currently loaded in processor
        bool processorHasFile = currentProcessorPath.isNotEmpty();
        if (!fileLoaded && !processorHasFile) {
            selectAndPreview(indexToSelect);
        } else if (processorHasFile) {
            // Sync editor state with processor without reloading
            fileLoaded = true;
            if (pluginProcessor) {
                midiFileDuration = pluginProcessor->getFileDuration();
                midiFileBpm = pluginProcessor->getFileBpm();
            }
        }
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

    if (!file.existsAsFile()) {
        info.analyzed = true;  // Mark as analyzed to avoid retrying
        return;
    }

    // Capture file size
    info.fileSize = file.getSize();

    juce::FileInputStream stream(file);
    if (!stream.openedOk()) {
        info.analyzed = true;
        return;
    }

    juce::MidiFile midiFile;
    if (!midiFile.readFrom(stream)) {
        info.analyzed = true;
        return;
    }

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
        if (sequence) {
            for (int i = 0; i < sequence->getNumEvents(); i++) {
                auto* event = sequence->getEventPointer(i);
                double eventTime = event->message.getTimeStamp();
                if (eventTime > maxTime) maxTime = eventTime;
                // For note-on, also check note-off time
                if (event->message.isNoteOn() && event->noteOffObject) {
                    double offTime = event->noteOffObject->message.getTimeStamp();
                    if (offTime > maxTime) maxTime = offTime;
                }
            }
        }
    }

    // Round duration to nearest bar (4 beats) for clean looping
    double bpm = info.bpm > 0 ? info.bpm : 120.0;
    double beatsPerSecond = bpm / 60.0;
    double totalBeats = maxTime * beatsPerSecond;
    double bars = std::ceil(totalBeats / 4.0);  // Round up to nearest bar
    if (bars < 1.0) bars = 1.0;
    double roundedBeats = bars * 4.0;
    info.duration = roundedBeats / beatsPerSecond;
    info.durationBeats = roundedBeats;
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
        // Filter by selected library
        if (selectedLibraryIndex == 0) {
            // "All" - show all files (no library filter)
        } else if (selectedLibraryIndex == 1) {
            // "Favorites" - only show favorites
            if (!file.favorite) continue;
        } else if (selectedLibraryIndex == 2) {
            // "Recently Played" - only show recently played
            bool isRecent = false;
            for (const auto& recentPath : recentlyPlayed) {
                if (recentPath == file.fullPath) {
                    isRecent = true;
                    break;
                }
            }
            if (!isRecent) continue;
        } else {
            // User library - filter by library name
            int libIndex = selectedLibraryIndex - 3;
            if (libIndex >= 0 && libIndex < (int)libraries.size()) {
                if (file.libraryName != libraries[(size_t)libIndex].name) continue;
            }
        }

        // Check key filter - match key or relative key
        if (keyFilterCombo.getSelectedId() > 1) {
            bool matches = (file.key == keyFilter) || (file.relativeKey == keyFilter);
            if (!matches) continue;
        }

        // Check search filter (matches filename, key, or relative key)
        if (searchText.isNotEmpty()) {
            bool matchesSearch = file.fileName.toLowerCase().contains(searchText) ||
                                 file.key.toLowerCase().contains(searchText) ||
                                 file.relativeKey.toLowerCase().contains(searchText);
            if (!matchesSearch) continue;
        }

        // Skip files with 0 duration (empty or invalid MIDI files)
        if (file.analyzed && file.duration <= 0.0) continue;

        filteredFiles.push_back(file);
    }

    fileCountLabel.setText(juce::String((int)filteredFiles.size()) + " files", juce::dontSendNotification);
    fileListBox->updateContent();
    fileListBox->repaint();
}

void MIDIXplorerEditor::updateKeyFilterFromDetectedScales() {
    detectedKeys.clear();
    juce::StringArray allKeysAndRelatives;
    juce::StringArray allScales;

    for (const auto& file : allFiles) {
        // Add the relative key format (e.g., "C/Am")
        if (file.relativeKey.isNotEmpty() && file.relativeKey != "---" &&
            !allKeysAndRelatives.contains(file.relativeKey)) {
            allKeysAndRelatives.add(file.relativeKey);
        }
        // Add the full scale name (e.g., "C Japanese", "D Minor")
        if (file.key.isNotEmpty() && file.key != "---" &&
            !allScales.contains(file.key)) {
            allScales.add(file.key);
        }
    }

    // Bubble sort keys by musical key order (C, C#, D, etc.)
    for (int i = 0; i < allKeysAndRelatives.size() - 1; i++) {
        for (int j = i + 1; j < allKeysAndRelatives.size(); j++) {
            if (getKeyOrder(allKeysAndRelatives[i]) > getKeyOrder(allKeysAndRelatives[j])) {
                allKeysAndRelatives.getReference(i).swapWith(allKeysAndRelatives.getReference(j));
            }
        }
    }

    // Sort scales by key order too
    for (int i = 0; i < allScales.size() - 1; i++) {
        for (int j = i + 1; j < allScales.size(); j++) {
            if (getKeyOrder(allScales[i]) > getKeyOrder(allScales[j])) {
                allScales.getReference(i).swapWith(allScales.getReference(j));
            }
        }
    }

    keyFilterCombo.clear();
    keyFilterCombo.addItem("All Keys", 1);

    // Add separator and relative keys section
    int id = 2;
    keyFilterCombo.addSeparator();
    for (const auto& key : allKeysAndRelatives) {
        keyFilterCombo.addItem(key, id++);
    }

    // Add separator and scales section
    keyFilterCombo.addSeparator();
    for (const auto& scale : allScales) {
        keyFilterCombo.addItem(scale, id++);
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

void MIDIXplorerEditor::saveRecentlyPlayed() {
    auto settingsDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("MIDIXplorer");
    auto recentFile = settingsDir.getChildFile("recently_played.txt");

    juce::String content;
    for (const auto& path : recentlyPlayed) {
        content += path + "\n";
    }
    recentFile.replaceWithText(content);
}

void MIDIXplorerEditor::loadRecentlyPlayed() {
    auto settingsDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("MIDIXplorer");
    auto recentFile = settingsDir.getChildFile("recently_played.txt");

    recentlyPlayed.clear();
    if (!recentFile.existsAsFile()) return;

    juce::StringArray paths;
    recentFile.readLines(paths);

    for (const auto& path : paths) {
        if (path.isNotEmpty()) {
            recentlyPlayed.push_back(path);
        }
    }
}

void MIDIXplorerEditor::addToRecentlyPlayed(const juce::String& filePath) {
    // Remove if already exists (to move to front)
    recentlyPlayed.erase(
        std::remove(recentlyPlayed.begin(), recentlyPlayed.end(), filePath),
        recentlyPlayed.end()
    );

    // Add to front
    recentlyPlayed.insert(recentlyPlayed.begin(), filePath);

    // Keep max 50 recent files
    if (recentlyPlayed.size() > 50) {
        recentlyPlayed.resize(50);
    }

    saveRecentlyPlayed();
    libraryListBox.repaint();  // Update file count
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
        return;  // Just show empty view, no message
    }

    int noteRange = highestNote - lowestNote + 1;
    if (noteRange <= 0) noteRange = 1;

    float noteHeight = (float)bounds.getHeight() / (float)noteRange;
    float pixelsPerSecond = (float)bounds.getWidth() * zoomLevel / (float)totalDuration;
    float scrollPixels = scrollOffset * pixelsPerSecond;

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

            float x = (float)(startTime * pixelsPerSecond) - scrollPixels;
            float w = (float)((endTime - startTime) * pixelsPerSecond);
            if (w < 2.0f) w = 2.0f;
            float y = bounds.getHeight() - (noteNum - lowestNote + 1) * noteHeight;

            // Skip notes outside visible area
            if (x + w < 0 || x > bounds.getWidth()) continue;

            // Note color based on velocity, highlight if hovered
            int velocity = msg.getVelocity();
            float brightness = 0.5f + (velocity / 254.0f) * 0.5f;

            if (noteNum == hoveredNote) {
                // Highlighted note
                g.setColour(juce::Colours::orange);
            } else {
                g.setColour(juce::Colour::fromHSV(0.55f, 0.7f, brightness, 1.0f));
            }
            g.fillRoundedRectangle(x, y + 1, w, noteHeight - 2, 2.0f);
        }
    }

    // Draw playhead
    if (playPosition >= 0 && playPosition <= 1.0) {
        float xPos = (float)(playPosition * totalDuration * pixelsPerSecond) - scrollPixels;
        g.setColour(juce::Colours::white);
        g.drawVerticalLine((int)xPos, 0.0f, (float)bounds.getHeight());
    }

    // Draw note name tooltip if hovering
    if (hoveredNote >= 0) {
        juce::String noteName = getNoteNameFromMidi(hoveredNote);

        // Draw tooltip background
        g.setFont(12.0f);
        int textWidth = g.getCurrentFont().getStringWidth(noteName) + 10;
        int textHeight = 18;

        int tooltipX = hoverPos.x + 10;
        int tooltipY = hoverPos.y - textHeight - 5;

        // Keep tooltip in bounds
        if (tooltipX + textWidth > bounds.getWidth()) {
            tooltipX = hoverPos.x - textWidth - 10;
        }
        if (tooltipY < 0) {
            tooltipY = hoverPos.y + 10;
        }

        g.setColour(juce::Colour(0xff333333));
        g.fillRoundedRectangle((float)tooltipX, (float)tooltipY, (float)textWidth, (float)textHeight, 4.0f);
        g.setColour(juce::Colours::white);
        g.drawText(noteName, tooltipX, tooltipY, textWidth, textHeight, juce::Justification::centred);
    }
}

void MIDIXplorerEditor::MIDINoteViewer::setSequence(const juce::MidiMessageSequence* seq, double duration) {
    sequence = seq;
    totalDuration = duration > 0 ? duration : 1.0;
    
    // Reset zoom and scroll when loading a new file
    zoomLevel = 1.0f;
    scrollOffset = 0.0f;

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

juce::String MIDIXplorerEditor::MIDINoteViewer::getNoteNameFromMidi(int midiNote) {
    static const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    int octave = (midiNote / 12) - 1;
    int noteIndex = midiNote % 12;
    return juce::String(noteNames[noteIndex]) + juce::String(octave);
}

void MIDIXplorerEditor::MIDINoteViewer::mouseMove(const juce::MouseEvent& e) {
    if (sequence == nullptr || sequence->getNumEvents() == 0) {
        hoveredNote = -1;
        return;
    }

    auto bounds = getLocalBounds();
    float noteRangeF = (float)(highestNote - lowestNote + 1);
    if (noteRangeF < 1.0f) noteRangeF = 12.0f;
    float noteHeight = (float)bounds.getHeight() / noteRangeF;

    // Find which note the mouse is over
    int newHoveredNote = -1;
    hoverPos = e.getPosition();

    for (int i = 0; i < sequence->getNumEvents(); i++) {
        auto* event = sequence->getEventPointer(i);
        if (!event->message.isNoteOn()) continue;

        int noteNumber = event->message.getNoteNumber();
        double startTime = event->message.getTimeStamp();
        double endTime = startTime + 0.1;  // Default duration

        if (event->noteOffObject != nullptr) {
            endTime = event->noteOffObject->message.getTimeStamp();
        }

        // Calculate note rectangle
        float x = (float)(startTime / totalDuration) * bounds.getWidth();
        float width = (float)((endTime - startTime) / totalDuration) * bounds.getWidth();
        float y = (float)(highestNote - noteNumber) / noteRangeF * bounds.getHeight();

        if (width < 2.0f) width = 2.0f;

        // Check if mouse is inside this note
        if (e.x >= x && e.x <= x + width &&
            e.y >= y && e.y <= y + noteHeight) {
            newHoveredNote = noteNumber;
            break;
        }
    }

    if (newHoveredNote != hoveredNote) {
        hoveredNote = newHoveredNote;
        repaint();
    }
}

void MIDIXplorerEditor::MIDINoteViewer::mouseExit(const juce::MouseEvent& /*e*/) {
    if (hoveredNote != -1) {
        hoveredNote = -1;
        repaint();
    }
}

void MIDIXplorerEditor::MIDINoteViewer::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) {
    // Zoom with mouse wheel, centered on cursor position
    if (wheel.deltaY != 0) {
        float oldZoom = zoomLevel;
        float zoomDelta = wheel.deltaY * 0.3f;
        float newZoom = juce::jlimit(0.5f, 8.0f, zoomLevel + zoomDelta);

        if (newZoom != oldZoom) {
            // Calculate the time position under the cursor
            float cursorX = (float)event.x;
            float width = (float)getWidth();
            float pixelsPerSecondOld = width * oldZoom / (float)totalDuration;
            float timeAtCursor = (cursorX + scrollOffset * pixelsPerSecondOld) / pixelsPerSecondOld;

            // Update zoom
            zoomLevel = newZoom;

            // Adjust scroll offset so the time at cursor stays at cursor position
            float pixelsPerSecondNew = width * newZoom / (float)totalDuration;
            float newScrollPixels = timeAtCursor * pixelsPerSecondNew - cursorX;
            scrollOffset = newScrollPixels / pixelsPerSecondNew;

            // Clamp scroll offset to valid range
            float maxScroll = (float)totalDuration * (1.0f - 1.0f / zoomLevel);
            scrollOffset = juce::jlimit(0.0f, juce::jmax(0.0f, maxScroll), scrollOffset);

            repaint();
        }
    }
}

void MIDIXplorerEditor::MIDINoteViewer::mouseMagnify(const juce::MouseEvent& event, float scaleFactor) {
    // Pinch-to-zoom on trackpad, centered on cursor position
    float oldZoom = zoomLevel;
    float newZoom = juce::jlimit(0.5f, 8.0f, zoomLevel * scaleFactor);

    if (newZoom != oldZoom) {
        // Calculate the time position under the cursor
        float cursorX = (float)event.x;
        float width = (float)getWidth();
        float pixelsPerSecondOld = width * oldZoom / (float)totalDuration;
        float timeAtCursor = (cursorX + scrollOffset * pixelsPerSecondOld) / pixelsPerSecondOld;

        // Update zoom
        zoomLevel = newZoom;

        // Adjust scroll offset so the time at cursor stays at cursor position
        float pixelsPerSecondNew = width * newZoom / (float)totalDuration;
        float newScrollPixels = timeAtCursor * pixelsPerSecondNew - cursorX;
        scrollOffset = newScrollPixels / pixelsPerSecondNew;

        // Clamp scroll offset to valid range
        float maxScroll = (float)totalDuration * (1.0f - 1.0f / zoomLevel);
        scrollOffset = juce::jlimit(0.0f, juce::jmax(0.0f, maxScroll), scrollOffset);

        repaint();
    }
}

void MIDIXplorerEditor::LibraryListModel::paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) {
    // Special libraries: 0=All, 1=Favorites, 2=Recently Played
    juce::String name;
    int fileCount = 0;
    juce::String icon;
    bool isLibraryScanning = false;

    if (row == 0) {
        name = "All";
        fileCount = (int)owner.allFiles.size();
        icon = juce::String::fromUTF8("\u2630");  // Hamburger menu icon
        // Check if any files are in analysis queue
        isLibraryScanning = !owner.analysisQueue.empty();
    } else if (row == 1) {
        name = "Favorites";
        fileCount = 0;
        for (const auto& f : owner.allFiles) {
            if (f.favorite) fileCount++;
        }
        icon = juce::String::fromUTF8("\u2605");  // Star icon
    } else if (row == 2) {
        name = "Recently Played";
        fileCount = (int)owner.recentlyPlayed.size();
        icon = juce::String::fromUTF8("\u23F1");  // Stopwatch icon
    } else {
        int libIndex = row - 3;
        if (libIndex < 0 || libIndex >= (int)owner.libraries.size()) return;
        auto& lib = owner.libraries[(size_t)libIndex];
        name = lib.name;
        fileCount = lib.fileCount;
        icon = juce::String::fromUTF8("\u{1F4BE}");  // Hard drive/disk icon
        isLibraryScanning = lib.isScanning;
        // Also check if any files from this library are in analysis queue
        if (!isLibraryScanning) {
            for (size_t idx : owner.analysisQueue) {
                if (idx < owner.allFiles.size() && owner.allFiles[idx].libraryName == lib.name) {
                    isLibraryScanning = true;
                    break;
                }
            }
        }
    }

    bool isSelected = (row == owner.selectedLibraryIndex);

    if (isSelected) {
        g.setColour(juce::Colour(0xff0078d4));  // Blue highlight for selected
        g.fillRect(0, 0, w, h);
    } else if (selected) {
        g.setColour(juce::Colour(0xff3a3a3a));
        g.fillRect(0, 0, w, h);
    }

    // Icon
    g.setColour(row < 3 ? juce::Colours::orange : juce::Colours::grey);
    g.setFont(14.0f);
    g.drawText(icon, 4, 0, 20, h, juce::Justification::centred);

    // Name
    g.setColour(juce::Colours::white);
    g.setFont(13.0f);
    g.drawText(name, 26, 0, w - 80, h, juce::Justification::centredLeft);

    // File count or spinner
    if (isLibraryScanning) {
        // Draw animated spinner
        float centerX = w - 25.0f;
        float centerY = h / 2.0f;
        float radius = 6.0f;

        // Draw spinning dots
        for (int i = 0; i < 8; i++) {
            float angle = juce::MathConstants<float>::twoPi * i / 8.0f;
            float dotX = centerX + radius * std::cos(angle);
            float dotY = centerY + radius * std::sin(angle);

            // Fade based on distance from current frame
            int dist = (owner.spinnerFrame - i + 8) % 8;
            float alpha = 1.0f - (dist * 0.12f);
            g.setColour(juce::Colour(0xff00cc88).withAlpha(alpha));
            g.fillEllipse(dotX - 2.0f, dotY - 2.0f, 4.0f, 4.0f);
        }
    } else {
        g.setColour(juce::Colours::grey);
        g.setFont(11.0f);
        g.drawText(juce::String(fileCount), w - 50, 0, 45, h, juce::Justification::centredRight);
    }
}

void MIDIXplorerEditor::LibraryListModel::listBoxItemClicked(int row, const juce::MouseEvent& e) {
    int totalRows = 3 + (int)owner.libraries.size();
    if (row < 0 || row >= totalRows) return;

    // Left click - select library and filter
    if (e.mods.isLeftButtonDown()) {
        owner.selectedLibraryIndex = row;
        owner.filterFiles();
        owner.libraryListBox.repaint();
        return;
    }

    // Right click - context menu for user libraries only
    if (e.mods.isRightButtonDown() && row >= 3) {
        int libIndex = row - 3;
        if (libIndex < 0 || libIndex >= (int)owner.libraries.size()) return;

        juce::PopupMenu menu;
        menu.addItem(1, "Reveal in Finder");
        menu.addItem(2, "Refresh");
        menu.addItem(3, owner.libraries[(size_t)libIndex].enabled ? "Disable" : "Enable");
        menu.addItem(4, "Remove");

        menu.showMenuAsync(juce::PopupMenu::Options(), [this, libIndex](int result) {
            if (result == 1) {
                owner.revealInFinder(owner.libraries[(size_t)libIndex].path);
            } else if (result == 2) {
                owner.refreshLibrary((size_t)libIndex);
            } else if (result == 3) {
                owner.libraries[(size_t)libIndex].enabled = !owner.libraries[(size_t)libIndex].enabled;
                owner.saveLibraries();
                owner.scanLibraries();
            } else if (result == 4) {
                // Remove all cached files from this library
                juce::String libName = owner.libraries[(size_t)libIndex].name;
                owner.allFiles.erase(
                    std::remove_if(owner.allFiles.begin(), owner.allFiles.end(),
                        [&libName](const MIDIXplorerEditor::MIDIFileInfo& f) { 
                            return f.libraryName == libName; 
                        }),
                    owner.allFiles.end());
                
                // Remove the library
                owner.libraries.erase(owner.libraries.begin() + libIndex);
                owner.saveLibraries();
                owner.saveFileCache();  // Update cache after removing files
                owner.libraryListBox.updateContent();
                owner.filterFiles();
                owner.updateKeyFilterFromDetectedScales();
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

    // Favorite star icon
    float starX = 12.0f;
    float starY = h / 2.0f;
    float starSize = 7.0f;

    juce::Path starPath;
    // Draw 5-point star
    for (int i = 0; i < 5; i++) {
        float outerAngle = juce::MathConstants<float>::twoPi * i / 5.0f - juce::MathConstants<float>::halfPi;
        float innerAngle = outerAngle + juce::MathConstants<float>::twoPi / 10.0f;

        float outerX = starX + starSize * std::cos(outerAngle);
        float outerY = starY + starSize * std::sin(outerAngle);
        float innerX = starX + starSize * 0.4f * std::cos(innerAngle);
        float innerY = starY + starSize * 0.4f * std::sin(innerAngle);

        if (i == 0) {
            starPath.startNewSubPath(outerX, outerY);
        } else {
            starPath.lineTo(outerX, outerY);
        }
        starPath.lineTo(innerX, innerY);
    }
    starPath.closeSubPath();

    if (file.favorite) {
        g.setColour(juce::Colour(0xffffcc00));  // Gold/yellow for favorites
        g.fillPath(starPath);
    } else {
        g.setColour(juce::Colour(0xff666666));  // Grey outline
        g.strokePath(starPath, juce::PathStrokeType(1.0f));
    }

    // Key badge - show spinner if not yet analyzed
    g.setColour(juce::Colour(0xff3a3a3a));
    g.fillRoundedRectangle(28.0f, 6.0f, 70.0f, 20.0f, 4.0f);

    if (!file.analyzed) {
        // Draw animated spinner for files being analyzed
        float centerX = 63.0f;
        float centerY = 16.0f;
        float radius = 5.0f;

        for (int i = 0; i < 8; i++) {
            float angle = juce::MathConstants<float>::twoPi * i / 8.0f;
            float dotX = centerX + radius * std::cos(angle);
            float dotY = centerY + radius * std::sin(angle);

            int dist = (owner.spinnerFrame - i + 8) % 8;
            float alpha = 1.0f - (dist * 0.12f);
            g.setColour(juce::Colour(0xff00cc88).withAlpha(alpha));
            g.fillEllipse(dotX - 1.5f, dotY - 1.5f, 3.0f, 3.0f);
        }
    } else {
        g.setColour(juce::Colours::cyan);
        g.setFont(11.0f);
        g.drawText(file.key, 28, 6, 70, 20, juce::Justification::centred);
    }

    // Circle of Fifths relative key (Parent Major / Relative minor)
    if (file.relativeKey.isNotEmpty()) {
        g.setColour(juce::Colour(0xffff9944));  // Orange for relative key
        g.setFont(12.0f);
        g.drawText("(" + file.relativeKey + ")", 102, 6, 70, 20, juce::Justification::centredLeft);
    }

    // File name
    g.setColour(juce::Colours::white);
    g.setFont(13.0f);
    g.drawText(file.fileName, 175, 0, w - 480, h, juce::Justification::centredLeft);

    // Instrument name
    g.setColour(juce::Colour(0xffaaaaff));
    g.drawText(file.instrument, w - 380, 0, 130, h, juce::Justification::centredLeft);

    // File size
    g.setColour(juce::Colour(0xff888888));
    g.setFont(10.0f);
    juce::String sizeStr;
    if (file.fileSize >= 1024 * 1024) {
        sizeStr = juce::String(file.fileSize / (1024.0 * 1024.0), 1) + " MB";
    } else if (file.fileSize >= 1024) {
        sizeStr = juce::String(file.fileSize / 1024.0, 1) + " KB";
    } else {
        sizeStr = juce::String(file.fileSize) + " B";
    }
    g.drawText(sizeStr, w - 190, 0, 50, h, juce::Justification::centredRight);

    // Duration in seconds
    g.setColour(juce::Colours::grey);
    g.setFont(11.0f);
    // Draw BPM
    juce::String bpmStr = juce::String((int)file.bpm) + " bpm";
    g.drawText(bpmStr, w - 130, 0, 60, h, juce::Justification::centredRight);

    // Draw duration (bars and time) with elapsed time for playing file
    int bars = (int)(file.durationBeats / 4.0);
    int mins = (int)(file.duration) / 60;
    int secs = (int)(file.duration) % 60;

    if (row == owner.selectedFileIndex && owner.isPlaying && owner.fileLoaded) {
        // Show elapsed / total time for the playing file
        double elapsed = owner.currentPlaybackPosition * file.duration;
        int elapsedMins = (int)(elapsed) / 60;
        int elapsedSecs = (int)(elapsed) % 60;
        juce::String timeStr = juce::String::formatted("%d:%02d / %d:%02d", elapsedMins, elapsedSecs, mins, secs);
        g.setColour(juce::Colour(0xff00cc88));  // Green for playing
        g.drawText(timeStr, w - 95, 0, 90, h, juce::Justification::centredRight);
    } else {
        juce::String durationStr = juce::String::formatted("%dbar %d:%02d", bars, mins, secs);
        g.drawText(durationStr, w - 85, 0, 80, h, juce::Justification::centredRight);
    }

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

    // Check if star area clicked (first 24 pixels)
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

void MIDIXplorerEditor::quantizeMidi() {
    if (selectedFileIndex < 0 || selectedFileIndex >= (int)filteredFiles.size()) return;

    juce::String filePath = filteredFiles[(size_t)selectedFileIndex].fullPath;

    int modeId = quantizeCombo.getSelectedId();
    if (modeId == 0) return;

    // Get the grid interval in beats (4 beats = 1 bar in 4/4)
    double gridBeats = 0.0;
    double swingRatio = 0.5;  // 0.5 = no swing, >0.5 = swing (first note longer)
    bool isSwing = false;
    bool isMixed = false;
    double secondaryGridBeats = 0.0;  // For mixed modes

    switch (modeId) {
        // Straight notes
        case 1: gridBeats = 4.0; break;       // 1/1 Note (whole note = 4 beats)
        case 2: gridBeats = 2.0; break;       // 1/2 Note
        case 3: gridBeats = 1.0; break;       // 1/4 Note
        case 4: gridBeats = 0.5; break;       // 1/8 Note
        case 5: gridBeats = 0.25; break;      // 1/16 Note
        case 6: gridBeats = 0.125; break;     // 1/32 Note
        case 7: gridBeats = 0.0625; break;    // 1/64 Note

        // Triplets (3 notes in space of 2)
        case 8: gridBeats = 4.0 / 3.0; break;   // 1/2 Triplet (1/3)
        case 9: gridBeats = 2.0 / 3.0; break;   // 1/4 Triplet (1/6)
        case 10: gridBeats = 1.0 / 3.0; break;  // 1/8 Triplet (1/12)
        case 11: gridBeats = 0.5 / 3.0; break;  // 1/16 Triplet (1/24)
        case 12: gridBeats = 0.25 / 3.0; break; // 1/32 Triplet (1/48)
        case 13: gridBeats = 0.125 / 3.0; break;// 1/64 Triplet (1/96)
        case 14: gridBeats = 0.0625 / 3.0; break;// 1/128 Triplet (1/192)

        // 1/16 Swing (A=54%, B=58%, C=62%, D=66%, E=71%, F=75%)
        case 15: gridBeats = 0.25; isSwing = true; swingRatio = 0.54; break;
        case 16: gridBeats = 0.25; isSwing = true; swingRatio = 0.58; break;
        case 17: gridBeats = 0.25; isSwing = true; swingRatio = 0.62; break;
        case 18: gridBeats = 0.25; isSwing = true; swingRatio = 0.66; break;
        case 19: gridBeats = 0.25; isSwing = true; swingRatio = 0.71; break;
        case 20: gridBeats = 0.25; isSwing = true; swingRatio = 0.75; break;

        // 1/8 Swing (A=54%, B=58%, C=62%, D=66%, E=71%, F=75%)
        case 21: gridBeats = 0.5; isSwing = true; swingRatio = 0.54; break;
        case 22: gridBeats = 0.5; isSwing = true; swingRatio = 0.58; break;
        case 23: gridBeats = 0.5; isSwing = true; swingRatio = 0.62; break;
        case 24: gridBeats = 0.5; isSwing = true; swingRatio = 0.66; break;
        case 25: gridBeats = 0.5; isSwing = true; swingRatio = 0.71; break;
        case 26: gridBeats = 0.5; isSwing = true; swingRatio = 0.75; break;

        // Tuplets
        case 27: gridBeats = 1.0 / 5.0; break;  // 5-Tuplet/4 (5 in space of 4)
        case 28: gridBeats = 0.5 / 5.0; break;  // 5-Tuplet/8
        case 29: gridBeats = 1.0 / 7.0; break;  // 7-Tuplet
        case 30: gridBeats = 1.0 / 9.0; break;  // 9-Tuplet

        // Mixed modes
        case 31: // 1/16 & 1/16 Triplet
            isMixed = true;
            gridBeats = 0.25;
            secondaryGridBeats = 0.25 / 3.0;  // 1/16 triplet
            break;
        case 32: // 1/16 & 1/8 Triplet
            isMixed = true;
            gridBeats = 0.25;
            secondaryGridBeats = 0.5 / 3.0;  // 1/8 triplet
            break;
        case 33: // 1/8 & 1/8 Triplet
            isMixed = true;
            gridBeats = 0.5;
            secondaryGridBeats = 0.5 / 3.0;  // 1/8 triplet
            break;

        default: return;
    }

    if (gridBeats <= 0.0) return;

    // Load the original MIDI file
    juce::File midiFile(filePath);
    juce::FileInputStream inputStream(midiFile);
    if (!inputStream.openedOk()) return;

    juce::MidiFile originalMidi;
    if (!originalMidi.readFrom(inputStream)) return;

    // Create quantized MIDI file
    juce::MidiFile quantizedMidi;
    short timeFormat = originalMidi.getTimeFormat();
    quantizedMidi.setTicksPerQuarterNote(timeFormat > 0 ? timeFormat : 480);

    double ticksPerBeat = (double)(timeFormat > 0 ? timeFormat : 480);
    double gridTicks = gridBeats * ticksPerBeat;
    double secondaryGridTicks = secondaryGridBeats * ticksPerBeat;

    for (int t = 0; t < originalMidi.getNumTracks(); t++) {
        const juce::MidiMessageSequence* track = originalMidi.getTrack(t);
        if (!track) continue;

        juce::MidiMessageSequence newTrack;

        for (int i = 0; i < track->getNumEvents(); i++) {
            auto* event = track->getEventPointer(i);
            if (!event) continue;

            juce::MidiMessage msg = event->message;
            double originalTime = msg.getTimeStamp();
            double newTime = originalTime;

            if (msg.isNoteOn() || msg.isNoteOff()) {
                if (isMixed) {
                    // For mixed modes, snap to nearest grid (primary or secondary)
                    double nearestPrimary = std::round(originalTime / gridTicks) * gridTicks;
                    double nearestSecondary = std::round(originalTime / secondaryGridTicks) * secondaryGridTicks;

                    double diffPrimary = std::abs(originalTime - nearestPrimary);
                    double diffSecondary = std::abs(originalTime - nearestSecondary);

                    newTime = (diffPrimary <= diffSecondary) ? nearestPrimary : nearestSecondary;
                } else if (isSwing) {
                    // For swing, quantize to swing grid
                    double pairLength = gridTicks * 2.0;  // Two notes form a swing pair
                    double pairIndex = std::floor(originalTime / pairLength);
                    double posInPair = originalTime - (pairIndex * pairLength);

                    // First beat of pair is longer (swingRatio)
                    double firstBeatEnd = pairLength * swingRatio;

                    if (posInPair < firstBeatEnd) {
                        // Snap to first beat (start of pair)
                        double distToStart = posInPair;
                        double distToSecond = firstBeatEnd - posInPair;
                        newTime = (distToStart <= distToSecond)
                            ? pairIndex * pairLength
                            : pairIndex * pairLength + firstBeatEnd;
                    } else {
                        // Snap to second beat or next pair start
                        double distToSecond = posInPair - firstBeatEnd;
                        double distToNext = pairLength - posInPair;
                        newTime = (distToSecond <= distToNext)
                            ? pairIndex * pairLength + firstBeatEnd
                            : (pairIndex + 1) * pairLength;
                    }
                } else {
                    // Standard quantization - snap to nearest grid
                    newTime = std::round(originalTime / gridTicks) * gridTicks;
                }
            }

            msg.setTimeStamp(newTime);
            newTrack.addEvent(msg);
        }

        newTrack.updateMatchedPairs();
        quantizedMidi.addTrack(newTrack);
    }

    // Apply quantization only to the playback sequence (temporary, not saved to file)
    // Convert to seconds for playback
    quantizedMidi.convertTimestampTicksToSeconds();

    // Merge all tracks into playback sequence
    playbackSequence.clear();
    for (int t = 0; t < quantizedMidi.getNumTracks(); t++) {
        auto* track = quantizedMidi.getTrack(t);
        if (track) {
            for (int i = 0; i < track->getNumEvents(); i++) {
                playbackSequence.addEvent(track->getEventPointer(i)->message);
            }
        }
    }
    playbackSequence.updateMatchedPairs();
    playbackSequence.sort();

    // Update duration from quantized sequence
    midiFileDuration = 0.0;
    for (int i = 0; i < playbackSequence.getNumEvents(); i++) {
        double t = playbackSequence.getEventPointer(i)->message.getTimeStamp();
        if (t > midiFileDuration) midiFileDuration = t;
    }

    // Reset playback position and send sequence to processor
    playbackNoteIndex = 0;
    currentPlaybackPosition = 0.0;

    if (pluginProcessor) {
        // Preserve the current playback timing when quantizing during playback
        double currentStartBeat = playbackStartBeat;
        double currentStartTime = playbackStartTime;
        bool wasPlaying = isPlaying;

        pluginProcessor->loadPlaybackSequence(playbackSequence, midiFileDuration, midiFileBpm, filteredFiles[(size_t)selectedFileIndex].fullPath);

        if (wasPlaying) {
            // Restore timing so playback continues from same position
            playbackStartBeat = currentStartBeat;
            playbackStartTime = currentStartTime;
            // Don't call resetPlayback() - just continue playing
            pluginProcessor->setPlaybackPlaying(true);
        }
    }

    // Update the MIDI note viewer with quantized sequence
    midiNoteViewer.setSequence(&playbackSequence, midiFileDuration);

    // Show brief confirmation
    juce::String modeName = quantizeCombo.getText();
    isQuantized = true;
    DBG("Quantized MIDI to: " << modeName << " (temporary)");
}
