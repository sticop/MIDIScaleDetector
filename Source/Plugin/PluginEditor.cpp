#include "PluginEditor.h"
#include "MIDIScalePlugin.h"
#include "BinaryData.h"
#include "../Version.h"
#include "../Standalone/ActivationDialog.h"
#include <set>
#include <unordered_map>

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

    constexpr int kTransposePresetValues[] = {36, 24, 12, 0, -12, -24, -36};

    int getTransposePresetValueForId(int id) {
        int index = id - 2;  // id 2..8 map to presets
        if (index < 0 || index >= (int)(sizeof(kTransposePresetValues) / sizeof(kTransposePresetValues[0])))
            return 0;
        return kTransposePresetValues[index];
    }

    int getTransposePresetIdForValue(int value) {
        for (int i = 0; i < (int)(sizeof(kTransposePresetValues) / sizeof(kTransposePresetValues[0])); ++i) {
            if (kTransposePresetValues[i] == value) {
                return i + 2;
            }
        }
        return 1;  // Custom
    }
}

MIDIXplorerEditor::MIDIXplorerEditor(juce::AudioProcessor& p)
    : AudioProcessorEditor(&p), licenseManager(LicenseManager::getInstance()) {

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

    // Content type filter dropdown (chords vs notes)
    contentTypeFilterCombo.addItem("All Types", 1);
    contentTypeFilterCombo.addItem("Chords Only", 2);
    contentTypeFilterCombo.addItem("Notes Only", 3);
    contentTypeFilterCombo.setSelectedId(1);
    contentTypeFilterCombo.onChange = [this]() { filterFiles(); };
    addAndMakeVisible(contentTypeFilterCombo);

    // Tag filter dropdown (extracted from filenames)
    tagFilterCombo.addItem("All Tags", 1);
    tagFilterCombo.setSelectedId(1);
    tagFilterCombo.onChange = [this]() { filterFiles(); };
    addAndMakeVisible(tagFilterCombo);

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
    searchBox.setTextToShowWhenEmpty("Search files, keys, instruments...", juce::Colours::grey);
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
        // Block playback if license is expired
        if (isLicenseExpiredOrTrialExpired()) {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "License Required",
                "MIDI preview is disabled. Please activate a license to enable playback.",
                "OK");
            return;
        }

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

    // Drag to DAW button - visible beside play button
    dragToDAWButton.setTooltip("Drag this to your DAW or Finder to export the MIDI file");
    addAndMakeVisible(dragToDAWButton);

    // Zoom is now handled by mouse wheel in the MIDI viewer

    // Transpose dropdown presets
    transposeComboBox.addItem("Custom", 1);
    for (int i = 0; i < (int)(sizeof(kTransposePresetValues) / sizeof(kTransposePresetValues[0])); ++i) {
        int value = kTransposePresetValues[i];
        juce::String label = (value > 0) ? ("+" + juce::String(value))
                                         : (value == 0 ? juce::CharPointer_UTF8("\xc2\xb1 0") : juce::String(value));
        transposeComboBox.addItem(label, i + 2);
    }
    transposeComboBox.setSelectedId(getTransposePresetIdForValue(0), juce::dontSendNotification);
    transposeComboBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff3a3a3a));
    transposeComboBox.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    transposeComboBox.setColour(juce::ComboBox::arrowColourId, juce::Colours::white);
    transposeComboBox.setTooltip("Transpose in semitones");
    transposeComboBox.onChange = [this]() {
        int selectedId = transposeComboBox.getSelectedId();
        if (selectedId <= 1) return;
        transposeAmount = getTransposePresetValueForId(selectedId);
        applyTransposeToPlayback();
    };
    addAndMakeVisible(transposeComboBox);

    // Transpose up button (+1 semitone)
    transposeUpButton.setButtonText("+");
    transposeUpButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3a3a3a));
    transposeUpButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    transposeUpButton.setTooltip("+1 semitone");
    transposeUpButton.onClick = [this]() {
        if (transposeAmount < 36) {
            transposeAmount++;
            updateTransposeComboSelection();
            applyTransposeToPlayback();
        }
    };
    addAndMakeVisible(transposeUpButton);

    // Transpose down button (-1 semitone)
    transposeDownButton.setButtonText("-");
    transposeDownButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3a3a3a));
    transposeDownButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    transposeDownButton.setTooltip("-1 semitone");
    transposeDownButton.onClick = [this]() {
        if (transposeAmount > -36) {
            transposeAmount--;
            updateTransposeComboSelection();
            applyTransposeToPlayback();
        }
    };
    addAndMakeVisible(transposeDownButton);

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

    // Velocity slider for velocity control
    velocityLabel.setText("Velocity:", juce::dontSendNotification);
    velocityLabel.setFont(juce::Font(12.0f));
    velocityLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    velocityLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(velocityLabel);

    velocitySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    velocitySlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    velocitySlider.setRange(0.0, 200.0, 1.0);  // 0% to 200%
    velocitySlider.setValue(100.0);  // Default 100%
    velocitySlider.setColour(juce::Slider::trackColourId, juce::Colour(0xff3a3a3a));
    velocitySlider.setColour(juce::Slider::thumbColourId, juce::Colours::orange);
    velocitySlider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    velocitySlider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff2a2a2a));
    velocitySlider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff444444));
    velocitySlider.onValueChange = [this]() {
        velocityFaderTouched = true;  // Mark that user has manually adjusted velocity
        if (pluginProcessor) {
            pluginProcessor->setVelocityScale((float)(velocitySlider.getValue() / 100.0));
        }
        midiNoteViewer.setVelocityScale((float)(velocitySlider.getValue() / 100.0));
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

    // Setup fullscreen toggle callback for MIDI viewer
    midiNoteViewer.onFullscreenToggle = [this]() {
        midiViewerFullscreen = !midiViewerFullscreen;
        midiNoteViewer.setFullscreen(midiViewerFullscreen);
        resized();
        repaint();
    };

    // Setup playhead jump callback for MIDI viewer
    midiNoteViewer.onPlayheadJump = [this](double position) {
        if (!fileLoaded || !pluginProcessor) return;

        // Update processor playback position (handles beat sync internally)
        pluginProcessor->seekToPosition(position);

        // Make sure we're playing
        if (!isPlaying) {
            isPlaying = true;
            playPauseButton.setButtonText(juce::String::fromUTF8("\u23F8"));  // Pause icon
            pluginProcessor->setPlaybackPlaying(true);
        }
    };

    // License management UI
    licenseManager.addListener(this);

    // Status bar activate button (visible when in trial or expired)
    statusBarActivateBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff4a9eff));
    statusBarActivateBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    statusBarActivateBtn.onClick = [this]() {
        showLicenseActivation();
    };
    addAndMakeVisible(statusBarActivateBtn);

    // Settings button (gear icon) for settings menu
    settingsButton.setButtonText(juce::String::fromUTF8("\u2699"));  // Gear icon ⚙
    settingsButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3a3a3a));
    settingsButton.setColour(juce::TextButton::textColourOffId, juce::Colours::lightgrey);
    settingsButton.setTooltip("Settings");
    settingsButton.onClick = [this]() { showSettingsMenu(); };
    addAndMakeVisible(settingsButton);

    // Initialize license manager and check trial status
    licenseManager.initializeTrial();
    licenseManager.checkTrialStatus();

    setSize(1200, 750);

    // Restore playback state from processor FIRST (in case editor was reopened while playing)
    // This sets pendingSelectedFilePath which loadLibraries will use
    if (pluginProcessor) {
        isPlaying = pluginProcessor->isPlaybackPlaying();
        if (isPlaying) {
            playPauseButton.setButtonText(juce::String::fromUTF8("\u23F8"));  // Pause icon
        }
        syncToHostToggle.setToggleState(pluginProcessor->isSyncToHost(), juce::dontSendNotification);

        // Restore the selected file path from processor (highest priority)
        juce::String lastPath = pluginProcessor->getCurrentFilePath();
        if (lastPath.isNotEmpty()) {
            pendingSelectedFilePath = lastPath;
        }
    }

    // Load saved libraries (will use pendingSelectedFilePath if processor didn't set one)
    loadLibraries();
    loadFavorites();
    loadRecentlyPlayed();

    startTimer(20); // 50 fps for smooth sync
}

MIDIXplorerEditor::~MIDIXplorerEditor() {
    licenseManager.removeListener(this);
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
        // Save all files, not just analyzed ones - to preserve progress
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
        fileObj->setProperty("mood", f.mood);
        fileObj->setProperty("analyzed", f.analyzed);
        fileObj->setProperty("containsChords", f.containsChords);
        fileObj->setProperty("containsSingleNotes", f.containsSingleNotes);

        // Save tags
        juce::Array<juce::var> tagsArray;
        for (const auto& tag : f.tags) {
            tagsArray.add(juce::var(tag));
        }
        fileObj->setProperty("tags", tagsArray);

        filesArray.add(juce::var(fileObj.get()));
    }
    root->setProperty("files", filesArray);
    root->setProperty("cacheVersion", 3);  // Bump version for tags support

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
                    info.mood = fileObj->getProperty("mood").toString();
                    info.analyzed = (bool)fileObj->getProperty("analyzed");
                    info.containsChords = (bool)fileObj->getProperty("containsChords");
                    info.containsSingleNotes = (bool)fileObj->getProperty("containsSingleNotes");

                    // If key is not set or is "---", try to extract from filename
                    if (info.key.isEmpty() || info.key == "---") {
                        juce::String extractedKey = extractKeyFromFilename(info.fileName);
                        if (extractedKey.isNotEmpty()) {
                            info.key = extractedKey;
                        }
                    }

                    // Load tags from cache or extract from filename
                    auto tagsVar = fileObj->getProperty("tags");
                    if (auto* tagsArray = tagsVar.getArray()) {
                        for (const auto& tagVar : *tagsArray) {
                            info.tags.add(tagVar.toString());
                        }
                    }
                    // If no tags in cache, extract from filename
                    if (info.tags.isEmpty()) {
                        info.tags = extractTagsFromFilename(info.fileName);
                    }

                    // Only add if file still exists
                    if (juce::File(info.fullPath).existsAsFile()) {
                        allFiles.push_back(info);

                        // Queue unanalyzed files for analysis to continue progress
                        if (!info.analyzed) {
                            analysisQueue.push_back(allFiles.size() - 1);
                        }
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

    // Save selected file path (prefer processor state if available)
    juce::String selectedPath = pluginProcessor ? pluginProcessor->getCurrentFilePath() : juce::String();
    if (selectedPath.isEmpty() && selectedFileIndex >= 0 && selectedFileIndex < (int)filteredFiles.size()) {
        selectedPath = filteredFiles[(size_t)selectedFileIndex].fullPath;
    }
    if (selectedPath.isNotEmpty()) {
        root->setProperty("selectedFilePath", selectedPath);
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

            // Restore selected file BEFORE scanning/filtering (only if not already set by processor)
            if (pendingSelectedFilePath.isEmpty()) {
                auto savedPath = obj->getProperty("selectedFilePath").toString();
                if (savedPath.isNotEmpty()) {
                    pendingSelectedFilePath = savedPath;
                }
            }

            // Then scan for any new files (this calls filterFiles which uses pendingSelectedFilePath)
            scanLibraries();
        }
    }
}

bool MIDIXplorerEditor::isLicenseExpiredOrTrialExpired() const {
    auto status = licenseManager.getCurrentStatus();
    return status == LicenseManager::LicenseStatus::Expired ||
           status == LicenseManager::LicenseStatus::TrialExpired ||
           status == LicenseManager::LicenseStatus::Invalid ||
           status == LicenseManager::LicenseStatus::Revoked;
}

int MIDIXplorerEditor::getLicenseStatusBarHeight() const {
    // Show bar for trial, expired, or invalid states
    auto status = licenseManager.getCurrentStatus();
    if (status == LicenseManager::LicenseStatus::Valid)
        return 0;  // No bar for valid license
    return 30;  // Height of status bar
}

void MIDIXplorerEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff1a1a1a));

    // Draw license status bar at top if needed
    int barHeight = getLicenseStatusBarHeight();
    if (barHeight > 0) {
        auto status = licenseManager.getCurrentStatus();
        juce::Colour barColour;
        juce::String statusText;

        if (isLicenseExpiredOrTrialExpired()) {
            barColour = juce::Colour(0xffcc3333);  // Red for expired
            if (status == LicenseManager::LicenseStatus::TrialExpired)
                statusText = "Trial Expired - MIDI preview disabled. Please activate a license.";
            else if (status == LicenseManager::LicenseStatus::Expired)
                statusText = "License Expired - MIDI preview disabled. Please renew your license.";
            else
                statusText = "License Invalid - MIDI preview disabled. Please activate a license.";
        } else if (status == LicenseManager::LicenseStatus::Trial) {
            int days = licenseManager.getTrialDaysRemaining();
            if (days <= 3)
                barColour = juce::Colour(0xffff8833);  // Orange for low trial days
            else
                barColour = juce::Colour(0xff3366aa);  // Blue for trial
            statusText = "Trial: " + juce::String(days) + " day" + (days != 1 ? "s" : "") + " remaining";
        } else {
            barColour = juce::Colour(0xff3366aa);  // Blue default
            statusText = "Unlicensed";
        }

        g.setColour(barColour);
        g.fillRect(0, 0, getWidth(), barHeight);

        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(13.0f)));
        g.drawText(statusText, 10, 0, getWidth() - 160, barHeight, juce::Justification::centredLeft);
        // Button is positioned in resized()
    }

    // Adjust sidebar background for status bar
    g.setColour(juce::Colour(0xff252525));
    g.fillRect(0, barHeight, 200, getHeight() - barHeight);

    // Separator line
    g.setColour(juce::Colour(0xff3a3a3a));
    g.drawVerticalLine(200, (float)barHeight, (float)getHeight());
}

void MIDIXplorerEditor::resized() {
    auto area = getLocalBounds();

    // Reserve space for license status bar at top and position activate button
    int barHeight = getLicenseStatusBarHeight();
    if (barHeight > 0) {
        statusBarActivateBtn.setVisible(!midiViewerFullscreen);
        statusBarActivateBtn.setBounds(getWidth() - 140, 3, 130, barHeight - 6);
        area.removeFromTop(barHeight);
    } else {
        statusBarActivateBtn.setVisible(false);
    }

    // Fullscreen mode for MIDI viewer
    if (midiViewerFullscreen) {
        // Hide all other components
        librariesLabel.setVisible(false);
        addLibraryBtn.setVisible(false);
        libraryListBox.setVisible(false);
        searchBox.setVisible(false);
        fileCountLabel.setVisible(false);
        keyFilterCombo.setVisible(false);
        contentTypeFilterCombo.setVisible(false);
        tagFilterCombo.setVisible(false);
        sortCombo.setVisible(false);
        velocitySlider.setVisible(false);
        velocityLabel.setVisible(false);
        playPauseButton.setVisible(false);
        transposeComboBox.setVisible(false);
        quantizeCombo.setVisible(false);
        fileListBox->setVisible(false);
        settingsButton.setVisible(false);

        // MIDI viewer takes entire area
        midiNoteViewer.setBounds(area.reduced(4));
        return;
    }

    // Normal mode - show all components
    librariesLabel.setVisible(true);
    addLibraryBtn.setVisible(true);
    libraryListBox.setVisible(true);
    searchBox.setVisible(true);
    fileCountLabel.setVisible(true);
    keyFilterCombo.setVisible(true);
    contentTypeFilterCombo.setVisible(true);
    tagFilterCombo.setVisible(true);
    sortCombo.setVisible(true);
    velocitySlider.setVisible(true);
    velocityLabel.setVisible(true);
    playPauseButton.setVisible(true);
    transposeComboBox.setVisible(true);
    quantizeCombo.setVisible(true);
    fileListBox->setVisible(true);
    settingsButton.setVisible(true);

    // Sidebar
    auto sidebar = area.removeFromLeft(200);
    auto sidebarTop = sidebar.removeFromTop(30);
    librariesLabel.setBounds(sidebarTop.removeFromLeft(100).reduced(8, 4));
    addLibraryBtn.setBounds(sidebarTop.reduced(4));
    libraryListBox.setBounds(sidebar.reduced(4));

    // Main content area
    area.removeFromLeft(1); // separator

    // Search bar row with settings button
    auto searchRow = area.removeFromTop(40);
    searchRow = searchRow.reduced(8, 6);
    settingsButton.setBounds(searchRow.removeFromRight(32));
    searchRow.removeFromRight(6);
    searchBox.setBounds(searchRow);

    // Controls bar with filters and options
    auto topBar = area.removeFromTop(32);
    topBar = topBar.reduced(8, 2);
    fileCountLabel.setBounds(topBar.removeFromLeft(70));
    keyFilterCombo.setBounds(topBar.removeFromLeft(105).reduced(2));
    contentTypeFilterCombo.setBounds(topBar.removeFromLeft(100).reduced(2));
    tagFilterCombo.setBounds(topBar.removeFromLeft(100).reduced(2));
    sortCombo.setBounds(topBar.removeFromLeft(110).reduced(2));
    // syncToHostToggle hidden

    // Bottom transport bar
    auto transport = area.removeFromBottom(40).reduced(8, 4);
    playPauseButton.setBounds(transport.removeFromLeft(40));
    transport.removeFromLeft(4);  // Small gap
    dragToDAWButton.setBounds(transport.removeFromLeft(85));  // Drag to DAW button
    // addToDAWButton hidden

    // Transpose controls on the right: [-] [dropdown] [+]
    int transposeComboWidth = 80;
    int transposeButtonSize = transport.getHeight() - 4;
    auto transposeArea = transport.removeFromRight(transposeComboWidth + transposeButtonSize * 2);
    transposeArea = transposeArea.reduced(0, 2);
    auto downArea = transposeArea.removeFromLeft(transposeArea.getHeight());
    auto upArea = transposeArea.removeFromRight(transposeArea.getHeight());
    transposeDownButton.setBounds(downArea);
    transposeComboBox.setBounds(transposeArea);
    transposeUpButton.setBounds(upArea);
    transport.removeFromRight(8);
    quantizeCombo.setBounds(transport.removeFromRight(145));
    transport.removeFromRight(4);
    // Velocity slider beside quantize
    velocitySlider.setBounds(transport.removeFromRight(80).reduced(0, 2));
    velocityLabel.setBounds(transport.removeFromRight(50));
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
                // Always count the file for this library's file count
                libraries[currentScanLibraryIndex].fileCount++;
                filesFound++;

                // Check for duplicates - only add to allFiles if not already present
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
                    // Try to extract key from filename first
                    juce::String extractedKey = extractKeyFromFilename(info.fileName);
                    info.key = extractedKey.isNotEmpty() ? extractedKey : "---";
                    info.tags = extractTagsFromFilename(info.fileName);
                    allFiles.push_back(info);

                    // Queue for analysis
                    analysisQueue.push_back(allFiles.size() - 1);
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
                analysisSaveCounter++;
            }
        }
        // Update UI if we analyzed files
        if (filesAnalyzed > 0) {
            updateKeyFilterFromDetectedScales();
            updateContentTypeFilter();
            updateTagFilter();
            fileListBox->repaint();
            libraryListBox.repaint();
        }

        // Save cache periodically during analysis (every 50 files) or when complete
        if (analysisSaveCounter >= 50 || (analysisQueue.empty() && !isScanningFiles)) {
            saveFileCache();
            analysisSaveCounter = 0;
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

        // Only update wasHostPlaying if we have a file loaded
        // This ensures the "host started" transition can fire when first file is selected
        if (fileLoaded) {
            wasHostPlaying = hostPlaying;
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
                suppressSelectionChange = true;
                fileListBox->selectRow(pendingFileIndex);
                suppressSelectionChange = false;
                selectedFileIndex = pendingFileIndex;
                dragToDAWButton.setCurrentFile(filteredFiles[(size_t)pendingFileIndex].fullPath);
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

        // Update currently playing notes display
        if (pluginProcessor) {
            midiNoteViewer.setPlayingNotes(pluginProcessor->getActiveNoteNumbers());
        }

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

    // Handle looping - just wrap the display time for UI
    // The actual loop reset (playbackNoteIndex, timing) is handled by the processor
    if (currentTime >= totalDuration) {
        // Don't modify playback state here - processor handles it
        // Just wrap currentTime for UI display purposes
        currentTime = std::fmod(currentTime, totalDuration);
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
            // Load the file first
            selectAndPreview(currentRow);
            // Then start playback if license valid
            if (!isLicenseExpiredOrTrialExpired()) {
                isPlaying = true;
                playPauseButton.setButtonText(juce::String::fromUTF8("\u23F8"));  // Pause icon
                if (pluginProcessor) {
                    pluginProcessor->setPlaybackPlaying(true);
                    pluginProcessor->resetPlayback();
                }
            }
            return true;
        }
    }

    return false;
}

void MIDIXplorerEditor::selectAndPreview(int row) {
    if (row < 0 || row >= (int)filteredFiles.size()) return;

    if (fileListBox->getSelectedRow() != row) {
        suppressSelectionChange = true;
        fileListBox->selectRow(row);
        suppressSelectionChange = false;
    }
    fileListBox->scrollToEnsureRowIsOnscreen(row);
    selectedFileIndex = row;

    // Update drag button with current file
    dragToDAWButton.setCurrentFile(filteredFiles[(size_t)row].fullPath);

    // Add to recently played
    addToRecentlyPlayed(filteredFiles[(size_t)row].fullPath);

    bool shouldSchedule = syncToHostToggle.getToggleState() && isHostPlaying() && isPlaying;
    if (shouldSchedule) {
        scheduleFileChangeTo(row);
        return;
    }

    // Don't auto-start playback - file should start paused
    // User must explicitly press play or use Enter key to start playback

    // Send note-offs for any currently playing notes before switching files
    if (pluginProcessor) {
        pluginProcessor->sendActiveNoteOffs();
    }

    // Reset velocity/volume to 100% on file change
    velocitySlider.setValue(100.0, juce::sendNotification);

    // Always load immediately for responsive browsing (but playback won't start if expired)
    // The file will sync to DAW tempo if sync is enabled
    loadSelectedFile();
}

void MIDIXplorerEditor::scheduleFileChangeTo(int row) {
    if (row < 0 || row >= (int)filteredFiles.size()) return;
    // Send all notes off immediately when scheduling a file change
    if (auto* pluginProcessor = dynamic_cast<MIDIScaleDetector::MIDIScalePlugin*>(&processor)) {
        for (int ch = 1; ch <= 16; ch++) {
            pluginProcessor->addMidiMessage(juce::MidiMessage::allNotesOff(ch));
        }
    }
    pendingFileChange = true;
    pendingFileIndex = row;
}

void MIDIXplorerEditor::loadSelectedFile() {
    isQuantized = false;  // Reset quantize when loading new file
    quantizeCombo.setSelectedId(100, juce::dontSendNotification);  // Reset to "Off"

    // Reset transpose when loading new file
    transposeAmount = 0;
    updateTransposeComboSelection();
    if (pluginProcessor) {
        pluginProcessor->setTransposeAmount(0);
    }

    // Reset velocity to use original MIDI file velocity (only apply scale if user touches fader)
    velocityFaderTouched = false;
    velocitySlider.setValue(100.0, juce::dontSendNotification);  // Reset slider visually
    if (pluginProcessor) {
        pluginProcessor->setVelocityScale(1.0f);  // Use original velocity (1.0 = no change)
    }
    midiNoteViewer.setVelocityScale(1.0f);

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

        // Only auto-start playback if host is playing and sync is enabled
        // Otherwise, file loads paused and user must explicitly press play
        bool shouldAutoPlay = syncToHostToggle.getToggleState() && isHostPlaying();
        if (shouldAutoPlay) {
            isPlaying = true;
            playPauseButton.setButtonText(juce::String::fromUTF8("\u23F8"));  // Pause icon
            pluginProcessor->setPlaybackPlaying(true);
        } else {
            // Keep current playing state (don't force start)
            pluginProcessor->setPlaybackPlaying(isPlaying);
        }
    }

    // If host is already playing, sync our timing to current position
    if (isHostPlaying()) {
        playbackStartBeat = getHostBeatPosition();
        playbackStartTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        playbackNoteIndex = 0;
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
        scheduleFileChangeTo(selectedFileIndex);
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

void MIDIXplorerEditor::updateTransposeComboSelection() {
    transposeComboBox.setSelectedId(getTransposePresetIdForValue(transposeAmount), juce::dontSendNotification);
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
        updateContentTypeFilter();
        updateTagFilter();
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
    updateContentTypeFilter();
    updateTagFilter();
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

        suppressSelectionChange = true;
        fileListBox->selectRow(indexToSelect);
        suppressSelectionChange = false;
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
                // Also update the MIDI viewer with the current sequence
                auto& seq = pluginProcessor->getPlaybackSequence();
                midiNoteViewer.setSequence(&seq, midiFileDuration);
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

    // Initialize chord detection flags (will be set after timestamp conversion)
    info.containsChords = false;
    info.containsSingleNotes = false;

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

    // Get mode degree string (e.g., "2nd", "4th", etc.)
    juce::String scaleName = scales[bestScaleIdx].name;
    juce::String modeInfo;

    if (scaleName == "Dorian") {
        modeInfo = " (Maj 2nd)";
    } else if (scaleName == "Phrygian") {
        modeInfo = " (Maj 3rd)";
    } else if (scaleName == "Lydian") {
        modeInfo = " (Maj 4th)";
    } else if (scaleName == "Mixolydian") {
        modeInfo = " (Maj 5th)";
    } else if (scaleName == "Minor") {
        modeInfo = " (Maj 6th)";
    } else if (scaleName == "Locrian") {
        modeInfo = " (Maj 7th)";
    } else if (scaleName == "Harmonic Min") {
        modeInfo = "";
    } else if (scaleName == "Melodic Min") {
        modeInfo = "";
    }

    info.key = juce::String(noteNames[bestKey]) + " " + scales[bestScaleIdx].name + modeInfo;

    // Calculate Circle of Fifths - show parent major key and relative minor
    // Each mode has a specific relationship to its parent major scale
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

    // Chord detection: analyze simultaneous notes (timestamps are now in seconds)
    // A chord is 2+ notes starting within a small time window (20ms)
    const double chordTimeWindow = 0.020;  // 20ms window for chord detection
    std::vector<std::pair<double, int>> noteEvents;  // timestamp, noteNumber

    for (int track = 0; track < midiFile.getNumTracks(); track++) {
        auto* sequence = midiFile.getTrack(track);
        if (sequence) {
            for (int i = 0; i < sequence->getNumEvents(); i++) {
                auto& msg = sequence->getEventPointer(i)->message;
                if (msg.isNoteOn() && msg.getVelocity() > 0) {
                    noteEvents.push_back({msg.getTimeStamp(), msg.getNoteNumber()});
                }
            }
        }
    }

    std::cerr << "[ANALYSIS DEBUG] File: " << info.fileName << " noteEvents.size()=" << noteEvents.size() << std::endl;
    for (const auto& ev : noteEvents) {
        std::cerr << "  timestamp=" << ev.first << " noteNumber=" << ev.second << std::endl;
    }

    if (!noteEvents.empty()) {
        // Sort by timestamp
        std::sort(noteEvents.begin(), noteEvents.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

        std::cerr << "[CHORD DEBUG] File: " << info.fileName << " has " << noteEvents.size() << " note events" << std::endl;

        int chordCount = 0;
        int singleNoteCount = 0;
        size_t i = 0;

        while (i < noteEvents.size()) {
            // Count notes within the time window
            double windowStart = noteEvents[i].first;
            size_t j = i;
            std::set<int> simultaneousNotes;  // Use set to avoid duplicate pitches

            while (j < noteEvents.size() && noteEvents[j].first - windowStart <= chordTimeWindow) {
                simultaneousNotes.insert(noteEvents[j].second);
                j++;
            }

            if (simultaneousNotes.size() >= 2) {
                chordCount++;  // 2+ notes = chord (interval or chord)
            } else {
                singleNoteCount++;  // Single notes = melodic content
            }

            // Move to next group
            i = j;
        }

        std::cerr << "[CHORD DEBUG] chordCount=" << chordCount << " singleNoteCount=" << singleNoteCount << std::endl;

        // Consider it a chord file if it has at least 1 chord event
        info.containsChords = (chordCount >= 1);
        // Consider it a single-note file if it has any single notes (melodic content)
        info.containsSingleNotes = (singleNoteCount >= 1);

        std::cerr << "[CHORD DEBUG] containsChords=" << info.containsChords << " containsSingleNotes=" << info.containsSingleNotes << std::endl;

        // Debug output
        std::cerr << "[ChordDetect] " << info.fileName << ": events=" << noteEvents.size()
                  << " chords=" << chordCount << " singles=" << singleNoteCount
                  << " containsChords=" << info.containsChords
                  << " containsSingleNotes=" << info.containsSingleNotes << std::endl;
    }

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

    // Detect mood based on key/scale, tempo, and velocity
    juce::String detectedMood = "Neutral";
    juce::String keyString = info.key;

    // Extract scale characteristics
    bool isMajor = keyString.contains("Major") || keyString.contains("Lydian") || keyString.contains("Mixolydian");
    bool isMinor = keyString.contains("Minor") || keyString.contains("Phrygian") || keyString.contains("Locrian") ||
                   keyString.contains("Harmonic") || keyString.contains("Melodic");
    bool isDorian = keyString.contains("Dorian");
    bool isBlues = keyString.contains("Blues");
    bool isPentatonic = keyString.contains("Pentatonic");
    bool isLydian = keyString.contains("Lydian");
    bool isPhrygian = keyString.contains("Phrygian");

    // Enhanced mood classification based on scale, tempo, and other factors
    double tempo = info.bpm;
    bool isSlow = tempo < 80;
    bool isMedium = tempo >= 80 && tempo < 120;
    bool isFast = tempo >= 120;

    if (isMajor) {
        if (isFast) {
            detectedMood = "Joyful";
        } else if (isSlow) {
            detectedMood = "Peaceful";
        } else {
            detectedMood = "Happy";
        }
    } else if (isMinor) {
        if (isFast) {
            detectedMood = "Intense";
        } else if (isSlow) {
            detectedMood = "Melancholic";
        } else {
            detectedMood = "Emotional";
        }
    } else if (isDorian) {
        if (isFast) {
            detectedMood = "Funky";
        } else {
            detectedMood = "Soulful";
        }
    } else if (isBlues) {
        if (isSlow) {
            detectedMood = "Sorrowful";
        } else {
            detectedMood = "Bluesy";
        }
    } else if (isLydian) {
        detectedMood = "Dreamy";
    } else if (isPhrygian) {
        detectedMood = "Exotic";
    } else if (isPentatonic) {
        detectedMood = "Ethereal";
    } else {
        detectedMood = "Mysterious";
    }

    info.mood = detectedMood;

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
    // Strip the count suffix (e.g., "C Major (5)" -> "C Major")
    if (keyFilter.containsChar('(') && keyFilter.endsWithChar(')')) {
        keyFilter = keyFilter.upToFirstOccurrenceOf(" (", false, false);
    }
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

        // Check content type filter (chords vs notes)
        int contentTypeFilter = contentTypeFilterCombo.getSelectedId();
        if (contentTypeFilter == 2) {
            // Chords Only - file must contain chords
            if (!file.containsChords) {
                std::cerr << "[FILTER DEBUG] Skipping (not chord): " << file.fileName << std::endl;
                continue;
            }
        } else if (contentTypeFilter == 3) {
            // Notes Only - file must contain single notes (melody) and NOT be primarily chords
            if (!file.containsSingleNotes || file.containsChords) {
                std::cerr << "[FILTER DEBUG] Skipping (not note): " << file.fileName << " containsChords=" << file.containsChords << " containsSingleNotes=" << file.containsSingleNotes << std::endl;
                continue;
            }
        }

        // Check tag filter
        if (tagFilterCombo.getSelectedId() > 1) {
            juce::String selectedTag = tagFilterCombo.getText();
            // Strip count suffix if present (e.g., "Bass (123)" -> "Bass")
            if (selectedTag.containsChar('(') && selectedTag.endsWithChar(')')) {
                selectedTag = selectedTag.upToFirstOccurrenceOf(" (", false, false);
            }
            if (!file.tags.contains(selectedTag)) continue;
        }

        // Check search filter (matches filename, key, relative key, instrument, or tags)
        if (searchText.isNotEmpty()) {
            bool matchesSearch = file.fileName.toLowerCase().contains(searchText) ||
                                 file.key.toLowerCase().contains(searchText) ||
                                 file.relativeKey.toLowerCase().contains(searchText) ||
                                 file.instrument.toLowerCase().contains(searchText);
            // Also check tags
            if (!matchesSearch) {
                for (const auto& tag : file.tags) {
                    if (tag.toLowerCase().contains(searchText)) {
                        matchesSearch = true;
                        break;
                    }
                }
            }
            if (!matchesSearch) continue;
        }

        // Skip files with 0 duration (empty or invalid MIDI files)
        if (file.analyzed && file.duration <= 0.0) continue;

        std::cerr << "[FILTER DEBUG] Included: " << file.fileName << " chords=" << file.containsChords << " notes=" << file.containsSingleNotes << std::endl;
        filteredFiles.push_back(file);
    }

    fileCountLabel.setText(juce::String((int)filteredFiles.size()) + " files", juce::dontSendNotification);
    fileListBox->updateContent();
    fileListBox->repaint();
    restoreSelectionFromCurrentFile();
}

void MIDIXplorerEditor::restoreSelectionFromCurrentFile() {
    if (filteredFiles.empty()) {
        selectedFileIndex = -1;
        return;
    }

    bool needsRestore = pendingSelectedFilePath.isNotEmpty() ||
                        selectedFileIndex < 0 ||
                        selectedFileIndex >= (int)filteredFiles.size();
    if (!needsRestore) return;

    juce::String processorPath = pluginProcessor ? pluginProcessor->getCurrentFilePath() : juce::String();
    juce::String targetPath = processorPath.isNotEmpty() ? processorPath : pendingSelectedFilePath;

    int indexToSelect = -1;
    if (targetPath.isNotEmpty()) {
        for (size_t i = 0; i < filteredFiles.size(); i++) {
            if (filteredFiles[i].fullPath == targetPath) {
                indexToSelect = (int)i;
                break;
            }
        }
    }

    if (indexToSelect < 0) {
        indexToSelect = 0;
    }

    suppressSelectionChange = true;
    fileListBox->selectRow(indexToSelect);
    suppressSelectionChange = false;
    fileListBox->scrollToEnsureRowIsOnscreen(indexToSelect);
    selectedFileIndex = indexToSelect;
    dragToDAWButton.setCurrentFile(filteredFiles[(size_t)indexToSelect].fullPath);

    if (targetPath == pendingSelectedFilePath &&
        indexToSelect >= 0 &&
        filteredFiles[(size_t)indexToSelect].fullPath == targetPath) {
        pendingSelectedFilePath = "";
    }

    if (processorPath.isNotEmpty() && pluginProcessor) {
        fileLoaded = true;
        midiFileDuration = pluginProcessor->getFileDuration();
        midiFileBpm = pluginProcessor->getFileBpm();
        auto& seq = pluginProcessor->getPlaybackSequence();
        midiNoteViewer.setSequence(&seq, midiFileDuration);
    }
}

void MIDIXplorerEditor::updateKeyFilterFromDetectedScales() {
    detectedKeys.clear();

    // Use maps to count files per key/scale
    std::map<juce::String, int> keyFileCounts;
    std::map<juce::String, int> scaleFileCounts;
    juce::StringArray allKeysAndRelatives;
    juce::StringArray allScales;

    for (const auto& file : allFiles) {
        // Add the relative key format (e.g., "C/Am")
        if (file.relativeKey.isNotEmpty() && file.relativeKey != "---") {
            keyFileCounts[file.relativeKey]++;
            if (!allKeysAndRelatives.contains(file.relativeKey)) {
                allKeysAndRelatives.add(file.relativeKey);
            }
        }
        // Add the full scale name (e.g., "C Japanese", "D Minor")
        if (file.key.isNotEmpty() && file.key != "---") {
            scaleFileCounts[file.key]++;
            if (!allScales.contains(file.key)) {
                allScales.add(file.key);
            }
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

    // Add separator and relative keys section with counts
    int id = 2;
    if (allKeysAndRelatives.size() > 0)
        keyFilterCombo.addSeparator();
    for (const auto& key : allKeysAndRelatives) {
        int count = keyFileCounts[key];
        keyFilterCombo.addItem(key + " (" + juce::String(count) + ")", id++);
    }

    // Add separator and scales section with counts
    if (allScales.size() > 0)
        keyFilterCombo.addSeparator();
    for (const auto& scale : allScales) {
        int count = scaleFileCounts[scale];
        keyFilterCombo.addItem(scale + " (" + juce::String(count) + ")", id++);
    }

    keyFilterCombo.setSelectedId(1);
}

void MIDIXplorerEditor::updateContentTypeFilter() {
    // Count files with chords and single notes
    int chordFileCount = 0;
    int noteFileCount = 0;

    std::cerr << "[FILTER DEBUG] updateContentTypeFilter: allFiles.size()=" << allFiles.size() << std::endl;
    for (const auto& file : allFiles) {
        if (file.analyzed) {
            std::cerr << "  File: " << file.fileName << " analyzed=" << file.analyzed << " containsChords=" << file.containsChords << " containsSingleNotes=" << file.containsSingleNotes << std::endl;
            if (file.containsChords) chordFileCount++;
            if (file.containsSingleNotes && !file.containsChords) noteFileCount++;
        }
    }
    std::cerr << "[FILTER DEBUG] Chord files: " << chordFileCount << ", Note files: " << noteFileCount << std::endl;

    // Update combo box items with counts
    int currentId = contentTypeFilterCombo.getSelectedId();
    contentTypeFilterCombo.clear();
    contentTypeFilterCombo.addItem("All Types", 1);
    contentTypeFilterCombo.addItem("Chords (" + juce::String(chordFileCount) + ")", 2);
    contentTypeFilterCombo.addItem("Notes (" + juce::String(noteFileCount) + ")", 3);
    contentTypeFilterCombo.setSelectedId(currentId, juce::dontSendNotification);
}

juce::StringArray MIDIXplorerEditor::extractTagsFromFilename(const juce::String& filename) {
    juce::StringArray tags;
    juce::String lowerFilename = filename.toLowerCase();

    // Instrument/Sound type tags
    static const std::vector<std::pair<juce::String, juce::String>> instrumentPatterns = {
        {"bass", "Bass"}, {"synth", "Synth"}, {"piano", "Piano"}, {"keys", "Keys"},
        {"pad", "Pad"}, {"lead", "Lead"}, {"arp", "Arp"}, {"guitar", "Guitar"},
        {"strings", "Strings"}, {"violin", "Violin"}, {"viola", "Viola"}, {"cello", "Cello"},
        {"flute", "Flute"}, {"organ", "Organ"}, {"pluck", "Pluck"}, {"brass", "Brass"},
        {"choir", "Choir"}, {"vocal", "Vocal"}, {"bell", "Bell"}, {"chimes", "Chimes"},
        {"mallet", "Mallet"}, {"percussion", "Percussion"}, {"drums", "Drums"},
        {"hihat", "Hi-Hat"}, {"hi-hat", "Hi-Hat"}, {"hi hat", "Hi-Hat"},
        {"kick", "Kick"}, {"snare", "Snare"}, {"clap", "Clap"},
        {"fx", "FX"}, {"riser", "Riser"}, {"sub", "Sub"}, {"stab", "Stab"},
        {"melody", "Melody"}, {"chords", "Chords"}, {"chord", "Chords"}
    };

    for (const auto& [pattern, tag] : instrumentPatterns) {
        if (lowerFilename.contains(pattern) && !tags.contains(tag)) {
            tags.add(tag);
        }
    }

    // Genre/Style tags
    static const std::vector<std::pair<juce::String, juce::String>> genrePatterns = {
        {"trance", "Trance"}, {"psy", "Psy"}, {"psytrance", "Psytrance"},
        {"house", "House"}, {"deephouse", "Deep House"}, {"deep house", "Deep House"},
        {"techno", "Techno"}, {"dubstep", "Dubstep"}, {"dnb", "DnB"}, {"drum and bass", "DnB"},
        {"edm", "EDM"}, {"electronica", "Electronica"}, {"ambient", "Ambient"},
        {"chillout", "Chillout"}, {"lofi", "Lo-Fi"}, {"lo-fi", "Lo-Fi"}, {"hellofi", "Lo-Fi"},
        {"trap", "Trap"}, {"hip hop", "Hip Hop"}, {"hiphop", "Hip Hop"},
        {"rock", "Rock"}, {"pop", "Pop"}, {"jazz", "Jazz"}, {"classical", "Classical"},
        {"flamenco", "Flamenco"}, {"flamenko", "Flamenco"},
        {"ballad", "Ballad"}, {"acid", "Acid"}, {"melodic", "Melodic"}
    };

    for (const auto& [pattern, tag] : genrePatterns) {
        if (lowerFilename.contains(pattern) && !tags.contains(tag)) {
            tags.add(tag);
        }
    }

    // Producer/Pack source tags (common sample pack indicators)
    static const std::vector<std::pair<juce::String, juce::String>> sourcePatterns = {
        {"zenhiser", "Zenhiser"}, {"adsr", "ADSR"}, {"[tps]", "TPS"},
        {"loopmasters", "Loopmasters"}, {"splice", "Splice"},
        {"vengeance", "Vengeance"}, {"native instruments", "NI"},
        {"cymatics", "Cymatics"}, {"kshmr", "KSHMR"}, {"black octopus", "Black Octopus"},
        {"production master", "Production Master"}, {"ghosthack", "Ghosthack"}
    };

    for (const auto& [pattern, tag] : sourcePatterns) {
        if (lowerFilename.contains(pattern) && !tags.contains(tag)) {
            tags.add(tag);
        }
    }

    // Detect numbered/indexed files (e.g., "01 Bass", "001 Midi")
    if (lowerFilename.length() >= 2) {
        bool startsWithNumber = true;
        int digitCount = 0;
        for (int i = 0; i < juce::jmin(3, lowerFilename.length()); i++) {
            if (juce::CharacterFunctions::isDigit(lowerFilename[i])) {
                digitCount++;
            } else if (lowerFilename[i] == ' ' || lowerFilename[i] == '_' || lowerFilename[i] == '-') {
                break;
            } else {
                startsWithNumber = false;
                break;
            }
        }
        if (startsWithNumber && digitCount >= 2 && !tags.contains("Indexed")) {
            tags.add("Indexed");
        }
    }

    return tags;
}

juce::String MIDIXplorerEditor::extractKeyFromFilename(const juce::String& filename) {
    // Remove file extension and convert to lowercase for matching
    juce::String name = filename.upToLastOccurrenceOf(".", false, false);
    juce::String lowerName = name.toLowerCase();

    // Note names in various formats
    static const std::vector<std::pair<juce::String, juce::String>> notePatterns = {
        // Sharp notes - check longer patterns first
        {"c#", "C#"}, {"c sharp", "C#"}, {"db", "Db"},
        {"d#", "D#"}, {"d sharp", "D#"}, {"eb", "Eb"},
        {"f#", "F#"}, {"f sharp", "F#"}, {"gb", "Gb"},
        {"g#", "G#"}, {"g sharp", "G#"}, {"ab", "Ab"},
        {"a#", "A#"}, {"a sharp", "A#"}, {"bb", "Bb"},
        // Natural notes (check after sharps/flats to avoid false matches)
        {"cmaj", "C"}, {"dmaj", "D"}, {"emaj", "E"}, {"fmaj", "F"},
        {"gmaj", "G"}, {"amaj", "A"}, {"bmaj", "B"},
    };

    // Scale/mode patterns - order matters (longer patterns first)
    static const std::vector<std::pair<juce::String, juce::String>> scalePatterns = {
        // Minor variations
        {"minor", "Minor"}, {"min", "Minor"}, {"m7", "Minor"}, {"m6", "Minor"},
        {"m9", "Minor"}, {"m ", "Minor"},
        // Major variations
        {"major", "Major"}, {"maj", "Major"},
        // Modes
        {"dorian", "Dorian"}, {"phrygian", "Phrygian"}, {"lydian", "Lydian"},
        {"mixolydian", "Mixolydian"}, {"locrian", "Locrian"}, {"aeolian", "Minor"},
        {"ionian", "Major"},
    };

    // Try to find key patterns in filename
    // Pattern formats seen in filenames:
    // - "120 D#min" or "120 Dmin"
    // - "128bpm Eminor"
    // - "123bpm Cm"
    // - "C#m" or "Cm"
    // - "02_Am_" or "04_Dm_"
    // - "Abmin" or "Abmaj"
    // - "G#m" or "Gm7"
    // - "F# Major" or "A Minor"

    juce::String foundNote;
    juce::String foundScale;

    // First, try to find patterns like "Am", "Cm", "F#m", "Bbm" etc (note + m for minor)
    // These are common short forms
    static const juce::StringArray shortMinorPatterns = {
        "c#m", "d#m", "f#m", "g#m", "a#m",  // Sharp minors
        "dbm", "ebm", "gbm", "abm", "bbm",  // Flat minors
        "cm", "dm", "em", "fm", "gm", "am", "bm"  // Natural minors
    };

    static const juce::StringArray shortMajorPatterns = {
        "cmaj", "dmaj", "emaj", "fmaj", "gmaj", "amaj", "bmaj",
        "c#maj", "d#maj", "f#maj", "g#maj", "a#maj",
        "dbmaj", "ebmaj", "gbmaj", "abmaj", "bbmaj"
    };

    // Map short patterns to proper note names
    auto getNoteFromShort = [](const juce::String& pattern) -> juce::String {
        juce::String note = pattern.substring(0, pattern.length() - 1);  // Remove 'm' or last char
        if (note.endsWith("maj")) note = note.dropLastCharacters(3);
        if (note.endsWith("m")) note = note.dropLastCharacters(1);

        // Capitalize properly
        if (note.length() == 1) return note.toUpperCase();
        if (note.length() == 2) return note.substring(0, 1).toUpperCase() + note.substring(1);
        return note;
    };

    // Look for short minor patterns (e.g., "Cm", "F#m")
    for (const auto& pattern : shortMinorPatterns) {
        // Check with word boundaries to avoid false matches
        int idx = lowerName.indexOf(pattern);
        if (idx >= 0) {
            // Check it's not part of a longer word (e.g., "bcm" shouldn't match "cm")
            bool validStart = (idx == 0) || !juce::CharacterFunctions::isLetter(lowerName[idx - 1]);
            int endIdx = idx + pattern.length();
            bool validEnd = (endIdx >= lowerName.length()) ||
                           (!juce::CharacterFunctions::isLetter(lowerName[endIdx]) ||
                            lowerName.substring(endIdx).startsWith("aj") ||  // Allow "maj" continuation
                            lowerName.substring(endIdx).startsWith("in"));   // Allow "minor" continuation

            if (validStart && validEnd) {
                foundNote = getNoteFromShort(pattern);
                foundScale = "Minor";
                break;
            }
        }
    }

    // If no minor found, look for major patterns
    if (foundNote.isEmpty()) {
        for (const auto& pattern : shortMajorPatterns) {
            int idx = lowerName.indexOf(pattern);
            if (idx >= 0) {
                bool validStart = (idx == 0) || !juce::CharacterFunctions::isLetter(lowerName[idx - 1]);
                int endIdx = idx + pattern.length();
                bool validEnd = (endIdx >= lowerName.length()) ||
                               !juce::CharacterFunctions::isLetter(lowerName[endIdx]);

                if (validStart && validEnd) {
                    foundNote = getNoteFromShort(pattern);
                    foundScale = "Major";
                    break;
                }
            }
        }
    }

    // Try longer patterns like "Eminor", "Aminor", "F#minor", "Abmaj"
    if (foundNote.isEmpty()) {
        // Check for patterns with "minor" or "major" suffix
        static const std::vector<std::pair<juce::String, juce::String>> longPatterns = {
            // Sharp/flat + minor/major
            {"c#minor", "C#"}, {"d#minor", "D#"}, {"f#minor", "F#"}, {"g#minor", "G#"}, {"a#minor", "A#"},
            {"dbminor", "Db"}, {"ebminor", "Eb"}, {"gbminor", "Gb"}, {"abminor", "Ab"}, {"bbminor", "Bb"},
            {"c#major", "C#"}, {"d#major", "D#"}, {"f#major", "F#"}, {"g#major", "G#"}, {"a#major", "A#"},
            {"dbmajor", "Db"}, {"ebmajor", "Eb"}, {"gbmajor", "Gb"}, {"abmajor", "Ab"}, {"bbmajor", "Bb"},
            // Natural + minor/major
            {"cminor", "C"}, {"dminor", "D"}, {"eminor", "E"}, {"fminor", "F"},
            {"gminor", "G"}, {"aminor", "A"}, {"bminor", "B"},
            {"cmajor", "C"}, {"dmajor", "D"}, {"emajor", "E"}, {"fmajor", "F"},
            {"gmajor", "G"}, {"amajor", "A"}, {"bmajor", "B"},
            // Short forms with min/maj
            {"c#min", "C#"}, {"d#min", "D#"}, {"f#min", "F#"}, {"g#min", "G#"}, {"a#min", "A#"},
            {"dbmin", "Db"}, {"ebmin", "Eb"}, {"gbmin", "Gb"}, {"abmin", "Ab"}, {"bbmin", "Bb"},
            {"cmin", "C"}, {"dmin", "D"}, {"emin", "E"}, {"fmin", "F"},
            {"gmin", "G"}, {"amin", "A"}, {"bmin", "B"},
        };

        for (const auto& [pattern, note] : longPatterns) {
            if (lowerName.contains(pattern)) {
                foundNote = note;
                foundScale = pattern.contains("minor") || pattern.contains("min") ? "Minor" : "Major";
                break;
            }
        }
    }

    // Try standalone note detection with context (e.g., "120bpm G", "128 F#")
    if (foundNote.isEmpty()) {
        // Look for patterns like "bpm X" or "BPM X" followed by note
        juce::StringArray tokens;
        tokens.addTokens(lowerName, " _-", "");

        for (int i = 0; i < tokens.size(); i++) {
            juce::String token = tokens[i];

            // Check if this token looks like a note (A-G optionally followed by # or b)
            if (token.length() >= 1 && token.length() <= 2) {
                juce::juce_wchar firstChar = token[0];
                if (firstChar >= 'a' && firstChar <= 'g') {
                    bool hasAccidental = token.length() == 2 && (token[1] == '#' || token[1] == 'b');

                    // Validate by checking surrounding context
                    bool hasContext = false;
                    if (i > 0) {
                        juce::String prev = tokens[i - 1];
                        // Previous token ends with "bpm" or is a number
                        if (prev.endsWithIgnoreCase("bpm") || prev.containsOnly("0123456789")) {
                            hasContext = true;
                        }
                    }
                    if (i < tokens.size() - 1) {
                        juce::String next = tokens[i + 1];
                        // Next token starts with scale indicator
                        if (next.startsWithIgnoreCase("min") || next.startsWithIgnoreCase("maj") ||
                            next.startsWithIgnoreCase("major") || next.startsWithIgnoreCase("minor")) {
                            hasContext = true;
                            foundScale = next.startsWithIgnoreCase("min") ? "Minor" : "Major";
                        }
                    }

                    if (hasContext || hasAccidental) {
                        foundNote = juce::String(token.substring(0, 1)).toUpperCase();
                        if (hasAccidental) {
                            foundNote += token[1];
                        }
                        if (foundScale.isEmpty()) {
                            foundScale = "Major";  // Default to major if not specified
                        }
                        break;
                    }
                }
            }
        }
    }

    // Return the found key or empty string
    if (foundNote.isNotEmpty()) {
        return foundNote + " " + foundScale;
    }

    return juce::String();  // Return empty if no key found
}

void MIDIXplorerEditor::updateTagFilter() {
    // Collect all unique tags from files
    allTags.clear();
    std::map<juce::String, int> tagCounts;

    for (const auto& file : allFiles) {
        for (const auto& tag : file.tags) {
            tagCounts[tag]++;
            if (!allTags.contains(tag)) {
                allTags.add(tag);
            }
        }
    }

    // Define tag categories
    struct TagCategory {
        juce::String name;
        juce::StringArray tags;
    };

    std::vector<TagCategory> categories = {
        {"Drums & Percussion", {"Drums", "Kick", "Snare", "Hi-Hat", "Clap", "Percussion"}},
        {"Keys & Melodic", {"Piano", "Keys", "Organ", "Bell", "Chimes", "Mallet"}},
        {"Synths", {"Synth", "Pad", "Lead", "Arp", "Pluck", "Stab"}},
        {"Bass", {"Bass", "Sub"}},
        {"Strings & Guitar", {"Strings", "Violin", "Viola", "Cello", "Guitar"}},
        {"Winds & Vocals", {"Flute", "Brass", "Choir", "Vocal"}},
        {"FX & Risers", {"FX", "Riser"}},
        {"Content Type", {"Melody", "Chords"}},
        {"Genre", {"Trance", "Psy", "Psytrance", "House", "Deep House", "Techno", "Dubstep",
                   "DnB", "EDM", "Electronica", "Ambient", "Chillout", "Lo-Fi", "Trap",
                   "Hip Hop", "Rock", "Pop", "Jazz", "Classical", "Flamenco", "Ballad", "Acid", "Melodic"}},
        {"Source", {"Zenhiser", "ADSR", "TPS", "Loopmasters", "Splice", "Vengeance", "NI",
                    "Cymatics", "KSHMR", "Black Octopus", "Production Master", "Ghosthack"}},
        {"Other", {"Indexed"}}
    };

    // Update combo box
    juce::String currentSelection = tagFilterCombo.getText();
    // Strip count suffix if present
    if (currentSelection.containsChar('(') && currentSelection.endsWithChar(')')) {
        currentSelection = currentSelection.upToFirstOccurrenceOf(" (", false, false);
    }

    tagFilterCombo.clear();
    tagFilterCombo.addItem("All Tags", 1);

    int itemId = 2;
    int selectedId = 1;

    // Track which tags have been added to avoid duplicates
    juce::StringArray addedTags;

    // Add tags by category
    for (const auto& category : categories) {
        bool hasTagsInCategory = false;

        // Check if any tags from this category exist in the file set
        for (const auto& tag : category.tags) {
            if (tagCounts.find(tag) != tagCounts.end()) {
                hasTagsInCategory = true;
                break;
            }
        }

        if (hasTagsInCategory) {
            tagFilterCombo.addSeparator();
            tagFilterCombo.addSectionHeading(category.name);

            for (const auto& tag : category.tags) {
                auto it = tagCounts.find(tag);
                if (it != tagCounts.end()) {
                    int count = it->second;
                    juce::String itemText = tag + " (" + juce::String(count) + ")";
                    tagFilterCombo.addItem(itemText, itemId);
                    if (tag == currentSelection) {
                        selectedId = itemId;
                    }
                    addedTags.add(tag);
                    itemId++;
                }
            }
        }
    }

    // Add any uncategorized tags at the end
    juce::StringArray uncategorizedTags;
    for (const auto& tag : allTags) {
        if (!addedTags.contains(tag)) {
            uncategorizedTags.add(tag);
        }
    }

    if (!uncategorizedTags.isEmpty()) {
        tagFilterCombo.addSeparator();
        tagFilterCombo.addSectionHeading("Uncategorized");
        for (const auto& tag : uncategorizedTags) {
            int count = tagCounts[tag];
            juce::String itemText = tag + " (" + juce::String(count) + ")";
            tagFilterCombo.addItem(itemText, itemId);
            if (tag == currentSelection) {
                selectedId = itemId;
            }
            itemId++;
        }
    }

    tagFilterCombo.setSelectedId(selectedId, juce::dontSendNotification);
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

    const bool hasSequence = (sequence != nullptr && sequence->getNumEvents() > 0);

    int displayLowest = hasSequence ? lowestNote : 0;
    int displayHighest = hasSequence ? highestNote : 127;
    int noteRange = displayHighest - displayLowest + 1;
    if (noteRange <= 0) noteRange = 1;

    float noteHeight = (float)bounds.getHeight() / (float)noteRange;

    // Reserve space for piano keyboard on left
    auto pianoArea = bounds.removeFromLeft(PIANO_WIDTH);
    auto noteArea = bounds;

    float pixelsPerSecond = (float)noteArea.getWidth() * zoomLevel / (float)totalDuration;
    float scrollPixels = scrollOffset * pixelsPerSecond;

    // Draw piano keyboard on left
    for (int note = displayLowest; note <= displayHighest; note++) {
        float y = bounds.getHeight() - (note - displayLowest + 1) * noteHeight;
        int noteInOctave = note % 12;
        bool isBlackKey = (noteInOctave == 1 || noteInOctave == 3 || noteInOctave == 6 || noteInOctave == 8 || noteInOctave == 10);

        // Check if this note is currently playing
        bool isPlaying = std::find(playingNotes.begin(), playingNotes.end(), note) != playingNotes.end();

        // Draw piano key
        if (isBlackKey) {
            g.setColour(isPlaying ? juce::Colours::orange : juce::Colour(0xff222222));
        } else {
            g.setColour(isPlaying ? juce::Colours::orange : juce::Colour(0xffe8e8e8));
        }
        g.fillRect(pianoArea.getX(), (int)y, PIANO_WIDTH - 2, (int)noteHeight);

        // Draw key border
        g.setColour(juce::Colour(0xff444444));
        g.drawRect(pianoArea.getX(), (int)y, PIANO_WIDTH - 2, (int)noteHeight);

        // Draw note name on C notes
        if (noteInOctave == 0 && noteHeight >= 8) {
            g.setColour(juce::Colours::black);
            g.setFont(juce::Font(juce::jmin(10.0f, noteHeight - 2)));
            g.drawText("C" + juce::String(note / 12 - 1), pianoArea.getX() + 2, (int)y, PIANO_WIDTH - 6, (int)noteHeight, juce::Justification::centredLeft);
        }
    }

    // Draw piano key background hints in note area
    for (int note = displayLowest; note <= displayHighest; note++) {
        float y = noteArea.getHeight() - (note - displayLowest + 1) * noteHeight;
        int noteInOctave = note % 12;
        bool isBlackKey = (noteInOctave == 1 || noteInOctave == 3 || noteInOctave == 6 || noteInOctave == 8 || noteInOctave == 10);
        if (isBlackKey) {
            g.setColour(juce::Colour(0xff161616));
            g.fillRect((float)noteArea.getX(), y, (float)noteArea.getWidth(), noteHeight);
        }
    }

    if (!hasSequence) {
        // Placeholder message when no file is loaded
        g.setColour(juce::Colours::grey);
        g.setFont(juce::Font(juce::FontOptions(14.0f)));
        g.drawText("No MIDI file selected", bounds, juce::Justification::centred);
    }

    // Draw notes
    if (hasSequence) {
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

                float x = noteArea.getX() + (float)(startTime * pixelsPerSecond) - scrollPixels;
                float w = (float)((endTime - startTime) * pixelsPerSecond);
                if (w < 2.0f) w = 2.0f;
                float y = noteArea.getHeight() - (noteNum - displayLowest + 1) * noteHeight;

                // Skip notes outside visible area
                if (x + w < noteArea.getX() || x > noteArea.getRight()) continue;

                // Clip notes that extend into piano area
                if (x < noteArea.getX()) {
                    w -= (noteArea.getX() - x);
                    x = noteArea.getX();
                    if (w < 2.0f) continue;
                }

                // Note color based on velocity: green (low) to red (high)
                int velocity = msg.getVelocity();
                int effectiveVelocity = juce::jlimit(0, 127, (int)std::round((float)velocity * velocityScale));
                float velocityNorm = juce::jlimit(0.0f, 1.0f, (float)effectiveVelocity / 127.0f);
                juce::Colour noteColour = juce::Colour(0xff22cc55)
                                              .interpolatedWith(juce::Colour(0xffcc3333), velocityNorm);

                if (noteNum == hoveredNote) {
                    // Highlighted note
                    g.setColour(juce::Colours::white);
                } else {
                    g.setColour(noteColour);
                }
                g.fillRoundedRectangle(x, y + 1, w, noteHeight - 2, 2.0f);
            }
        }
    }

    // Draw playhead
    if (playPosition >= 0 && playPosition <= 1.0) {
        float xPos = noteArea.getX() + (float)(playPosition * totalDuration * pixelsPerSecond) - scrollPixels;
        g.setColour(juce::Colours::white);
        g.drawVerticalLine((int)xPos, 0.0f, (float)noteArea.getHeight());
    }

    // Draw currently playing notes/chord display at top
    if (hasSequence && !playingNotes.empty()) {
        juce::String chordText = detectChordName(playingNotes);
        g.setColour(juce::Colour(0xcc000000));
        g.fillRoundedRectangle((float)noteArea.getX() + 5, 5.0f, (float)g.getCurrentFont().getStringWidth(chordText) + 16, 22.0f, 4.0f);
        g.setColour(juce::Colours::orange);
        g.setFont(14.0f);
        g.drawText(chordText, noteArea.getX() + 13, 5, noteArea.getWidth() - 20, 22, juce::Justification::left);
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

    // Draw selection rectangle if dragging
    if (isDraggingSelection) {
        auto selRect = juce::Rectangle<int>(selectionStart, selectionEnd);
        g.setColour(juce::Colour(0x400078d4));  // Semi-transparent blue fill
        g.fillRect(selRect);
        g.setColour(juce::Colour(0xff0078d4));  // Blue border
        g.drawRect(selRect, 2);
    }

    // Draw fullscreen toggle button in bottom right corner
    fullscreenBtnBounds = juce::Rectangle<int>(getWidth() - 34, getHeight() - 34, 28, 28);

    // Button background
    g.setColour(juce::Colour(0x99333333));
    g.fillRoundedRectangle(fullscreenBtnBounds.toFloat(), 4.0f);
    g.setColour(juce::Colour(0xff555555));
    g.drawRoundedRectangle(fullscreenBtnBounds.toFloat(), 4.0f, 1.0f);

    // Draw fullscreen/exit icon
    g.setColour(juce::Colours::white);
    auto iconBounds = fullscreenBtnBounds.reduced(6);
    if (fullscreenMode) {
        // Exit fullscreen icon (arrows pointing inward)
        g.drawLine((float)iconBounds.getX(), (float)iconBounds.getY() + 4, (float)iconBounds.getX() + 5, (float)iconBounds.getY(), 2.0f);
        g.drawLine((float)iconBounds.getX(), (float)iconBounds.getY() + 4, (float)iconBounds.getX() + 4, (float)iconBounds.getY() + 4, 2.0f);
        g.drawLine((float)iconBounds.getX(), (float)iconBounds.getY() + 4, (float)iconBounds.getX(), (float)iconBounds.getY(), 2.0f);

        g.drawLine((float)iconBounds.getRight(), (float)iconBounds.getBottom() - 4, (float)iconBounds.getRight() - 5, (float)iconBounds.getBottom(), 2.0f);
        g.drawLine((float)iconBounds.getRight(), (float)iconBounds.getBottom() - 4, (float)iconBounds.getRight() - 4, (float)iconBounds.getBottom() - 4, 2.0f);
        g.drawLine((float)iconBounds.getRight(), (float)iconBounds.getBottom() - 4, (float)iconBounds.getRight(), (float)iconBounds.getBottom(), 2.0f);
    } else {
        // Fullscreen icon (arrows pointing outward from corners)
        g.drawLine((float)iconBounds.getX(), (float)iconBounds.getY(), (float)iconBounds.getX() + 5, (float)iconBounds.getY(), 2.0f);
        g.drawLine((float)iconBounds.getX(), (float)iconBounds.getY(), (float)iconBounds.getX(), (float)iconBounds.getY() + 5, 2.0f);

        g.drawLine((float)iconBounds.getRight(), (float)iconBounds.getY(), (float)iconBounds.getRight() - 5, (float)iconBounds.getY(), 2.0f);
        g.drawLine((float)iconBounds.getRight(), (float)iconBounds.getY(), (float)iconBounds.getRight(), (float)iconBounds.getY() + 5, 2.0f);

        g.drawLine((float)iconBounds.getX(), (float)iconBounds.getBottom(), (float)iconBounds.getX() + 5, (float)iconBounds.getBottom(), 2.0f);
        g.drawLine((float)iconBounds.getX(), (float)iconBounds.getBottom(), (float)iconBounds.getX(), (float)iconBounds.getBottom() - 5, 2.0f);

        g.drawLine((float)iconBounds.getRight(), (float)iconBounds.getBottom(), (float)iconBounds.getRight() - 5, (float)iconBounds.getBottom(), 2.0f);
        g.drawLine((float)iconBounds.getRight(), (float)iconBounds.getBottom(), (float)iconBounds.getRight(), (float)iconBounds.getBottom() - 5, 2.0f);
    }
}

void MIDIXplorerEditor::MIDINoteViewer::resized() {
    // Update fullscreen button bounds when resized
    fullscreenBtnBounds = juce::Rectangle<int>(getWidth() - 34, getHeight() - 34, 28, 28);
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
    if (seq != nullptr && seq->getNumEvents() > 0) {
        for (int i = 0; i < seq->getNumEvents(); i++) {
            auto& msg = seq->getEventPointer(i)->message;
            if (msg.isNoteOn()) {
                int note = msg.getNoteNumber();
                if (note < lowestNote) lowestNote = note;
                if (note > highestNote) highestNote = note;
            }
        }
    } else {
        lowestNote = 0;
        highestNote = 127;
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

juce::String MIDIXplorerEditor::MIDINoteViewer::detectChordName(const std::vector<int>& notes) {
    if (notes.empty()) return "";
    if (notes.size() == 1) return getNoteNameFromMidi(notes[0]);

    static const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

    // Get unique pitch classes and find the bass note
    std::set<int> pitchClasses;
    int bassNote = 127;
    for (int n : notes) {
        pitchClasses.insert(n % 12);
        if (n < bassNote) bassNote = n;
    }

    if (pitchClasses.size() < 2) {
        // All same pitch class - just show the note
        return getNoteNameFromMidi(bassNote);
    }

    int bassClass = bassNote % 12;

    // Calculate intervals from bass note
    std::vector<int> intervals;
    for (int pc : pitchClasses) {
        int interval = (pc - bassClass + 12) % 12;
        if (interval > 0) intervals.push_back(interval);
    }
    std::sort(intervals.begin(), intervals.end());

    juce::String chordName = juce::String(noteNames[bassClass]);
    juce::String chordType;

    // Detect chord type based on intervals
    // Major triad: 4, 7
    // Minor triad: 3, 7
    // Diminished: 3, 6
    // Augmented: 4, 8
    // Sus2: 2, 7
    // Sus4: 5, 7
    // Major 7: 4, 7, 11
    // Minor 7: 3, 7, 10
    // Dominant 7: 4, 7, 10
    // Diminished 7: 3, 6, 9
    // Half-dim 7: 3, 6, 10
    // Add9: 4, 7, 14->2

    bool has2 = std::find(intervals.begin(), intervals.end(), 2) != intervals.end();
    bool has3 = std::find(intervals.begin(), intervals.end(), 3) != intervals.end();
    bool has4 = std::find(intervals.begin(), intervals.end(), 4) != intervals.end();
    bool has5 = std::find(intervals.begin(), intervals.end(), 5) != intervals.end();
    bool has6 = std::find(intervals.begin(), intervals.end(), 6) != intervals.end();
    bool has7 = std::find(intervals.begin(), intervals.end(), 7) != intervals.end();
    bool has8 = std::find(intervals.begin(), intervals.end(), 8) != intervals.end();
    bool has9 = std::find(intervals.begin(), intervals.end(), 9) != intervals.end();
    bool has10 = std::find(intervals.begin(), intervals.end(), 10) != intervals.end();
    bool has11 = std::find(intervals.begin(), intervals.end(), 11) != intervals.end();

    // Extended chords (7ths)
    if (has4 && has7 && has11) {
        chordType = "maj7";
    } else if (has3 && has7 && has10) {
        chordType = "m7";
    } else if (has4 && has7 && has10) {
        chordType = "7";
    } else if (has3 && has6 && has9) {
        chordType = "dim7";
    } else if (has3 && has6 && has10) {
        chordType = "m7b5";
    } else if (has4 && has8 && has11) {
        chordType = "maj7#5";
    }
    // Triads
    else if (has4 && has7) {
        chordType = "";  // Major (no suffix)
    } else if (has3 && has7) {
        chordType = "m";
    } else if (has3 && has6) {
        chordType = "dim";
    } else if (has4 && has8) {
        chordType = "aug";
    } else if (has2 && has7) {
        chordType = "sus2";
    } else if (has5 && has7) {
        chordType = "sus4";
    }
    // Power chord
    else if (has7 && !has3 && !has4) {
        chordType = "5";
    }
    // Two notes - interval name
    else if (intervals.size() == 1) {
        int interval = intervals[0];
        switch (interval) {
            case 1: chordType = "(m2)"; break;
            case 2: chordType = "(M2)"; break;
            case 3: chordType = "(m3)"; break;
            case 4: chordType = "(M3)"; break;
            case 5: chordType = "(P4)"; break;
            case 6: chordType = "(tri)"; break;
            case 7: chordType = "(P5)"; break;
            case 8: chordType = "(m6)"; break;
            case 9: chordType = "(M6)"; break;
            case 10: chordType = "(m7)"; break;
            case 11: chordType = "(M7)"; break;
            default: chordType = ""; break;
        }
    }
    // Unknown - just list notes
    else {
        juce::String noteList;
        for (size_t i = 0; i < notes.size(); i++) {
            if (i > 0) noteList += " ";
            noteList += getNoteNameFromMidi(notes[i]);
        }
        return noteList;
    }

    return chordName + chordType;
}

void MIDIXplorerEditor::MIDINoteViewer::mouseMove(const juce::MouseEvent& e) {
    // Check if hovering over fullscreen button - use hand cursor
    if (fullscreenBtnBounds.contains(e.getPosition())) {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    } else {
        setMouseCursor(juce::MouseCursor::CrosshairCursor);
    }

    if (sequence == nullptr || sequence->getNumEvents() == 0) {
        hoveredNote = -1;
        return;
    }

    auto bounds = getLocalBounds();
    auto noteArea = bounds.withTrimmedLeft(PIANO_WIDTH);
    float noteRangeF = (float)(highestNote - lowestNote + 1);
    if (noteRangeF < 1.0f) noteRangeF = 12.0f;
    float noteHeight = (float)noteArea.getHeight() / noteRangeF;
    float pixelsPerSecond = (float)noteArea.getWidth() * zoomLevel / (float)totalDuration;
    float scrollPixels = scrollOffset * pixelsPerSecond;

    // Find which note the mouse is over
    int newHoveredNote = -1;
    hoverPos = e.getPosition();

    if (e.x < noteArea.getX()) {
        if (newHoveredNote != hoveredNote) {
            hoveredNote = newHoveredNote;
            repaint();
        }
        return;
    }

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
        float x = noteArea.getX() + (float)(startTime * pixelsPerSecond) - scrollPixels;
        float width = (float)((endTime - startTime) * pixelsPerSecond);
        float y = noteArea.getHeight() - (noteNumber - lowestNote + 1) * noteHeight;

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
        float newZoom = juce::jlimit(0.5f, 32.0f, zoomLevel + zoomDelta);

        if (newZoom != oldZoom) {
            // Calculate the time position under the cursor (account for piano width)
            float cursorX = (float)(event.x - PIANO_WIDTH);
            float noteAreaWidth = (float)(getWidth() - PIANO_WIDTH);
            float pixelsPerSecondOld = noteAreaWidth * oldZoom / (float)totalDuration;
            float timeAtCursor = (cursorX + scrollOffset * pixelsPerSecondOld) / pixelsPerSecondOld;

            // Update zoom
            zoomLevel = newZoom;

            // Adjust scroll offset so the time at cursor stays at cursor position
            float pixelsPerSecondNew = noteAreaWidth * newZoom / (float)totalDuration;
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
    float newZoom = juce::jlimit(0.5f, 32.0f, zoomLevel * scaleFactor);

    if (newZoom != oldZoom) {
        // Calculate the time position under the cursor (account for piano width)
        float cursorX = (float)(event.x - PIANO_WIDTH);
        float noteAreaWidth = (float)(getWidth() - PIANO_WIDTH);
        float pixelsPerSecondOld = noteAreaWidth * oldZoom / (float)totalDuration;
        float timeAtCursor = (cursorX + scrollOffset * pixelsPerSecondOld) / pixelsPerSecondOld;

        // Update zoom
        zoomLevel = newZoom;

        // Adjust scroll offset so the time at cursor stays at cursor position
        float pixelsPerSecondNew = noteAreaWidth * newZoom / (float)totalDuration;
        float newScrollPixels = timeAtCursor * pixelsPerSecondNew - cursorX;
        scrollOffset = newScrollPixels / pixelsPerSecondNew;

        // Clamp scroll offset to valid range
        float maxScroll = (float)totalDuration * (1.0f - 1.0f / zoomLevel);
        scrollOffset = juce::jlimit(0.0f, juce::jmax(0.0f, maxScroll), scrollOffset);

        repaint();
    }
}

void MIDIXplorerEditor::MIDINoteViewer::mouseDown(const juce::MouseEvent& e) {
    // Check if clicking on fullscreen button
    if (fullscreenBtnBounds.contains(e.getPosition())) {
        if (onFullscreenToggle) {
            onFullscreenToggle();
        }
        return;
    }

    if (e.mods.isRightButtonDown()) {
        // Right-click: reset zoom completely
        resetZoom();
    } else if (e.mods.isLeftButtonDown()) {
        // Left-click: jump playhead to clicked position
        int clickX = e.getPosition().x;

        // Account for piano keyboard area on the left
        if (clickX > PIANO_WIDTH && totalDuration > 0) {
            float noteAreaWidth = (float)(getWidth() - PIANO_WIDTH);
            float pixelsPerSecond = noteAreaWidth * zoomLevel / (float)totalDuration;
            float scrollPixels = scrollOffset * pixelsPerSecond;

            // Calculate time position from click
            float clickedTime = (clickX - PIANO_WIDTH + scrollPixels) / pixelsPerSecond;
            double position = clickedTime / totalDuration;
            position = juce::jlimit(0.0, 1.0, position);

            // Call the callback to jump playhead
            if (onPlayheadJump) {
                onPlayheadJump(position);
            }
        }
    }
}

void MIDIXplorerEditor::MIDINoteViewer::mouseDrag(const juce::MouseEvent& e) {
    if (isDraggingSelection) {
        selectionEnd = e.getPosition();
        repaint();
    }
}

void MIDIXplorerEditor::MIDINoteViewer::mouseUp(const juce::MouseEvent& e) {
    if (isDraggingSelection) {
        isDraggingSelection = false;

        // Calculate selection rectangle - ensure proper ordering of coordinates
        int left = juce::jmin(selectionStart.x, selectionEnd.x);
        int right = juce::jmax(selectionStart.x, selectionEnd.x);
        auto selWidth = right - left;

        // Only zoom if selection is at least 10 pixels wide
        if (selWidth > 10) {
            // Account for piano keyboard area on the left
            float noteAreaWidth = (float)(getWidth() - PIANO_WIDTH);

            // Current pixels per second based on current zoom
            float currentPixelsPerSecond = noteAreaWidth * zoomLevel / (float)totalDuration;

            // Convert screen X positions to time values
            // Selection X is relative to component, need to subtract PIANO_WIDTH and account for scroll
            float selLeftX = (float)(left - PIANO_WIDTH);
            float selRightX = (float)(right - PIANO_WIDTH);

            // Clamp to note area bounds (0 to noteAreaWidth)
            selLeftX = juce::jmax(0.0f, selLeftX);
            selRightX = juce::jmin(noteAreaWidth, selRightX);

            // Convert pixel positions to time (accounting for current scroll)
            float selStartTime = scrollOffset + (selLeftX / currentPixelsPerSecond);
            float selEndTime = scrollOffset + (selRightX / currentPixelsPerSecond);
            float selDuration = selEndTime - selStartTime;

            if (selDuration > 0.01f) {
                // Calculate new zoom level: we want selDuration to fill the noteAreaWidth
                // pixelsPerSecond_new = noteAreaWidth * newZoom / totalDuration
                // We want: selDuration * pixelsPerSecond_new = noteAreaWidth
                // So: selDuration * noteAreaWidth * newZoom / totalDuration = noteAreaWidth
                // Therefore: newZoom = totalDuration / selDuration
                float newZoom = (float)totalDuration / selDuration;
                newZoom = juce::jlimit(0.5f, 32.0f, newZoom);

                // Scroll so that selStartTime is at the left edge of the view
                scrollOffset = selStartTime;
                zoomLevel = newZoom;

                // Clamp scroll offset to valid range
                float maxScroll = (float)totalDuration - ((float)totalDuration / zoomLevel);
                scrollOffset = juce::jlimit(0.0f, juce::jmax(0.0f, maxScroll), scrollOffset);
            }
        }
        else {
            // Treat as click-to-jump playhead
            if (onPlayheadJump) {
                int clickX = e.getPosition().x;
                if (clickX > PIANO_WIDTH) {
                    float noteAreaWidth = (float)(getWidth() - PIANO_WIDTH);
                    float pixelsPerSecond = noteAreaWidth * zoomLevel / (float)totalDuration;
                    float localX = (float)(clickX - PIANO_WIDTH);
                    localX = juce::jlimit(0.0f, juce::jmax(0.0f, noteAreaWidth), localX);
                    float timeAtClick = scrollOffset + (localX / pixelsPerSecond);
                    double position = timeAtClick / (float)totalDuration;
                    position = juce::jlimit(0.0, 1.0, position);
                    onPlayheadJump(position);
                }
            }
        }

        repaint();
    }
}

void MIDIXplorerEditor::LibraryListModel::paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) {
    // Special libraries: 0=All, 1=Favorites, 2=Recently Played
    juce::String name;
    int fileCount = 0;
    int processingCount = 0;
    int pendingCount = 0;
    bool showProcessingCounts = false;
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
        // Count only recently played files that actually exist in allFiles
        fileCount = 0;
        for (const auto& recentPath : owner.recentlyPlayed) {
            for (const auto& f : owner.allFiles) {
                if (f.fullPath == recentPath) {
                    fileCount++;
                    break;
                }
            }
        }
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

        int totalInLib = 0;
        int analyzedInLib = 0;
        for (const auto& f : owner.allFiles) {
            if (f.libraryName != lib.name) continue;
            totalInLib++;
            if (f.analyzed) analyzedInLib++;
        }
        processingCount = analyzedInLib;
        pendingCount = totalInLib;
        showProcessingCounts = (totalInLib > 0 && analyzedInLib < totalInLib);
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

    // File count, spinner, or processing/pending
    if (showProcessingCounts) {
        // Show as "analyzed / total"
        juce::String progressText = juce::String(processingCount) + " / " + juce::String(pendingCount);
        g.setColour(juce::Colour(0xff00cc88));
        g.setFont(11.0f);
        g.drawText(progressText, w - 80, 0, 75, h, juce::Justification::centredRight);
    } else if (isLibraryScanning) {
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
                owner.updateContentTypeFilter();
                owner.updateTagFilter();
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
    g.drawText(file.fileName, 175, 0, w - 530, h, juce::Justification::centredLeft);

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

    // Draw duration (time) with elapsed time for playing file
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
        juce::String durationStr = juce::String::formatted("%d:%02d", mins, secs);
        g.drawText(durationStr, w - 85, 0, 80, h, juce::Justification::centredRight);
    }

}

void MIDIXplorerEditor::FileListModel::selectedRowsChanged(int lastRowSelected) {
    if (owner.suppressSelectionChange) return;
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
        std::unordered_map<const juce::MidiMessageSequence::MidiEventHolder*, double> forcedNoteOffTimes;

        auto quantizeTime = [&](double originalTime) {
            if (gridTicks <= 0.0) return originalTime;

            if (isMixed) {
                // For mixed modes, snap to nearest grid (primary or secondary)
                double nearestPrimary = std::round(originalTime / gridTicks) * gridTicks;
                double nearestSecondary = std::round(originalTime / secondaryGridTicks) * secondaryGridTicks;

                double diffPrimary = std::abs(originalTime - nearestPrimary);
                double diffSecondary = std::abs(originalTime - nearestSecondary);

                return (diffPrimary <= diffSecondary) ? nearestPrimary : nearestSecondary;
            }

            if (isSwing) {
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
                    return (distToStart <= distToSecond)
                        ? pairIndex * pairLength
                        : pairIndex * pairLength + firstBeatEnd;
                }

                // Snap to second beat or next pair start
                double distToSecond = posInPair - firstBeatEnd;
                double distToNext = pairLength - posInPair;
                return (distToSecond <= distToNext)
                    ? pairIndex * pairLength + firstBeatEnd
                    : (pairIndex + 1) * pairLength;
            }

            // Standard quantization - snap to nearest grid
            return std::round(originalTime / gridTicks) * gridTicks;
        };

        for (int i = 0; i < track->getNumEvents(); i++) {
            auto* event = track->getEventPointer(i);
            if (!event) continue;

            juce::MidiMessage msg = event->message;
            double originalTime = msg.getTimeStamp();

            if (msg.isNoteOn()) {
                double newTime = quantizeTime(originalTime);
                msg.setTimeStamp(newTime);
                newTrack.addEvent(msg);

                if (event->noteOffObject != nullptr) {
                    double duration = event->noteOffObject->message.getTimeStamp() - originalTime;
                    if (duration < 1.0) duration = 1.0;
                    forcedNoteOffTimes[event->noteOffObject] = newTime + duration;
                }
            } else if (msg.isNoteOff()) {
                auto forced = forcedNoteOffTimes.find(event);
                double newTime = forced != forcedNoteOffTimes.end()
                    ? forced->second
                    : quantizeTime(originalTime);

                if (newTime < 0.0) newTime = 0.0;
                msg.setTimeStamp(newTime);
                newTrack.addEvent(msg);
            } else {
                newTrack.addEvent(msg);
            }
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

        pluginProcessor->loadPlaybackSequence(playbackSequence, midiFileDuration, midiFileBpm, filteredFiles[(size_t)selectedFileIndex].fullPath, true);

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


// License management callbacks
void MIDIXplorerEditor::licenseStatusChanged(LicenseManager::LicenseStatus status, const LicenseManager::LicenseInfo& info) {
    juce::ignoreUnused(info, status);
    // Refresh UI to update status bar
    resized();
    repaint();
}

void MIDIXplorerEditor::showLicenseActivation() {
    // Use the ActivationDialog popup instead of inline dialog
    ActivationDialog::showActivationDialog(this, [this](bool) {
        // Refresh UI after dialog closes
        repaint();
    });
}

void MIDIXplorerEditor::showSettingsMenu() {
    // Create the settings dialog component (Scaler-style modal)
    auto* settingsDialog = new SettingsDialogComponent(licenseManager, audioDeviceManager, volumeCallback, currentVolume);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(settingsDialog);
    options.dialogTitle = "About";
    options.dialogBackgroundColour = juce::Colour(0xff2a2a2a);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = false;
    options.resizable = false;

    auto* window = options.launchAsync();
    if (window) {
        window->centreWithSize(750, 500);
    }
}

void MIDIXplorerEditor::showHelpDialog(const juce::String& topic) {
    juce::String title;
    juce::String content;

    if (topic == "Getting Started") {
        title = "Getting Started with MIDI Xplorer";
        content = "Welcome to MIDI Xplorer!\n\n";
        content += "1. ADD A LIBRARY\n";
        content += "   Click the '+' button in the left sidebar to add a folder containing MIDI files.\n\n";
        content += "2. BROWSE YOUR FILES\n";
        content += "   Click on a library to see all MIDI files. Use the search box to filter.\n\n";
        content += "3. PREVIEW MIDI FILES\n";
        content += "   Select a file to see the piano roll. Press Space to play/stop.\n\n";
        content += "4. DRAG & DROP\n";
        content += "   Drag any MIDI file from the list directly into your DAW.\n\n";
        content += "5. FILTER BY KEY\n";
        content += "   Use the 'All Keys' dropdown to filter by detected musical key.\n\n";
        content += "6. FILTER BY TYPE\n";
        content += "   Use 'All Types' to filter between chords and single notes.";
    }
    else if (topic == "Keyboard Shortcuts") {
        title = "Keyboard Shortcuts";
        content = "PLAYBACK\n";
        content += "  Space       - Play/Stop current MIDI file\n";
        content += "  N           - Play next/random file\n";
        content += "  L           - Toggle loop mode\n\n";
        content += "NAVIGATION\n";
        content += "  Up/Down     - Navigate file list\n";
        content += "  Enter       - Load selected file\n";
        content += "  Cmd/Ctrl+F  - Focus search box\n\n";
        content += "VIEW\n";
        content += "  F           - Toggle fullscreen piano roll\n";
        content += "  Escape      - Exit fullscreen\n\n";
        content += "FILE MANAGEMENT\n";
        content += "  Star icon   - Toggle favorite";
    }
    else if (topic == "Scale Detection") {
        title = "Understanding Scale Detection";
        content = "MIDI Xplorer analyzes every note in your MIDI file to detect the most likely musical scale.\n\n";
        content += "HOW IT WORKS\n";
        content += "The analyzer counts notes in each pitch class and matches them against 25+ scale templates including:\n";
        content += "  - Major & Minor modes (Ionian, Dorian, Phrygian, etc.)\n";
        content += "  - Pentatonic scales\n";
        content += "  - Blues scales\n";
        content += "  - Exotic scales (Hungarian, Spanish, Arabic, etc.)\n\n";
        content += "RELATIVE KEY DISPLAY\n";
        content += "The relative key (e.g., 'C/Am') shows the parent major and relative minor.\n\n";
        content += "CHORD vs NOTE DETECTION\n";
        content += "Files with 3+ simultaneous notes are classified as containing chords.";
    }
    else if (topic == "MIDI Management") {
        title = "MIDI File Management";
        content = "ORGANIZING YOUR LIBRARY\n";
        content += "Add multiple library folders to organize MIDI files by project or genre.\n\n";
        content += "FAVORITES\n";
        content += "Click the star icon to mark files as favorites.\n\n";
        content += "RECENTLY PLAYED\n";
        content += "Quick access to files you've recently previewed.\n\n";
        content += "SEARCH & FILTER\n";
        content += "  - Search by filename, key, or instrument\n";
        content += "  - Filter by musical key\n";
        content += "  - Filter by content type (Chords/Notes)\n";
        content += "  - Sort by scale, duration, or name\n\n";
        content += "DRAG TO DAW\n";
        content += "Drag any file from the list directly into your DAW.";
    }
    else {
        title = "Help";
        content = "Select a help topic from the Settings menu.";
    }

    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::InfoIcon,
        title,
        content,
        "OK");
}

void MIDIXplorerEditor::showAboutDialog() {
    auto& license = licenseManager;
    auto info = license.getLicenseInfo();

    juce::String message;
    message += "Version: " + juce::String(MIDIXPLORER_VERSION_STRING) + "\n";
    message += "Build Date: " + juce::String(MIDIXPLORER_BUILD_DATE) + "\n\n";
    message += "A powerful MIDI file browser and analyzer\n";
    message += "with scale detection and DAW integration.\n\n";

    if (license.isLicenseValid()) {
        message += "License Status: ACTIVE\n";
        message += "Licensed to: " + info.customerName + "\n";
        message += "License type: " + info.licenseType + "\n";
    }
    else if (license.isInTrialPeriod()) {
        int days = license.getTrialDaysRemaining();
        message += "License Status: TRIAL\n";
        message += "Days remaining: " + juce::String(days) + "\n";
    }
    else {
        message += "License Status: EXPIRED\n";
        message += "Please activate a license to continue.\n";
    }

    message += "\n(c) 2024 MIDI Xplorer. All rights reserved.";

    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::InfoIcon,
        "About MIDI Xplorer",
        message,
        "OK");
}
