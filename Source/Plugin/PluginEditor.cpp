#include "PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>

// Note names for scale detection
static const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

// Detect key from MIDI file using note histogram
static void detectKeyFromMidi(const juce::MidiFile& midiFile, juce::String& outKey, juce::String& outScale, int& outNoteCount) {
    std::array<int, 12> noteHistogram = {0};
    outNoteCount = 0;
    
    for (int track = 0; track < midiFile.getNumTracks(); ++track) {
        auto* sequence = midiFile.getTrack(track);
        if (sequence == nullptr) continue;
        
        for (int i = 0; i < sequence->getNumEvents(); ++i) {
            auto& event = sequence->getEventPointer(i)->message;
            if (event.isNoteOn()) {
                int note = event.getNoteNumber() % 12;
                noteHistogram[(size_t)note]++;
                outNoteCount++;
            }
        }
    }
    
    if (outNoteCount == 0) {
        outKey = "Unknown";
        outScale = "";
        return;
    }
    
    // Major scale pattern: W-W-H-W-W-W-H (2-2-1-2-2-2-1)
    // Minor scale pattern: W-H-W-W-H-W-W (2-1-2-2-1-2-2)
    const int majorPattern[] = {0, 2, 4, 5, 7, 9, 11};
    const int minorPattern[] = {0, 2, 3, 5, 7, 8, 10};
    
    int bestScore = 0;
    int bestRoot = 0;
    bool isMajor = true;
    
    for (int root = 0; root < 12; ++root) {
        // Check major
        int majorScore = 0;
        for (int i = 0; i < 7; ++i) {
            majorScore += noteHistogram[(size_t)((root + majorPattern[i]) % 12)];
        }
        
        // Check minor
        int minorScore = 0;
        for (int i = 0; i < 7; ++i) {
            minorScore += noteHistogram[(size_t)((root + minorPattern[i]) % 12)];
        }
        
        if (majorScore > bestScore) {
            bestScore = majorScore;
            bestRoot = root;
            isMajor = true;
        }
        if (minorScore > bestScore) {
            bestScore = minorScore;
            bestRoot = root;
            isMajor = false;
        }
    }
    
    outKey = noteNames[bestRoot];
    outScale = isMajor ? "Major" : "Minor";
}

MIDIXplorerEditor::MIDIXplorerEditor(juce::AudioProcessor& p)
    : AudioProcessorEditor(&p)
{
    listModel = std::make_unique<FileListModel>(*this);
    
    // Title
    titleLabel.setText("MIDI Xplorer", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(24.0f));
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);
    
    // Status
    statusLabel.setText("Add a folder to scan for MIDI files", juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(statusLabel);
    
    // Buttons
    addFolderBtn.setButtonText("Add Folder");
    addFolderBtn.onClick = [this]() { addFolder(); };
    addAndMakeVisible(addFolderBtn);
    
    refreshBtn.setButtonText("Refresh");
    refreshBtn.onClick = [this]() { scanForMidiFiles(); };
    addAndMakeVisible(refreshBtn);
    
    playBtn.setButtonText("▶ Play");
    playBtn.onClick = [this]() { playSelectedFile(); };
    addAndMakeVisible(playBtn);
    
    stopBtn.setButtonText("■ Stop");
    stopBtn.onClick = [this]() { stopPlayback(); };
    addAndMakeVisible(stopBtn);
    
    // File list
    fileListBox.setModel(listModel.get());
    fileListBox.setRowHeight(28);
    fileListBox.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff2a2a2a));
    addAndMakeVisible(fileListBox);
    
    // Detail panel
    detailTitle.setText("Select a file", juce::dontSendNotification);
    detailTitle.setFont(juce::FontOptions(16.0f));
    detailTitle.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(detailTitle);
    
    detailKey.setText("Key: -", juce::dontSendNotification);
    detailKey.setColour(juce::Label::textColourId, juce::Colour(0xFF8AB4F8));
    addAndMakeVisible(detailKey);
    
    detailNotes.setText("Notes: -", juce::dontSendNotification);
    detailNotes.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(detailNotes);
    
    detailDuration.setText("Duration: -", juce::dontSendNotification);
    detailDuration.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(detailDuration);
    
    // Add default folders
    auto home = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
    scanFolders.add(home.getChildFile("Music").getFullPathName());
    scanFolders.add(home.getChildFile("Downloads").getFullPathName());
    
    // Try to get default MIDI output
    auto midiOutputs = juce::MidiOutput::getAvailableDevices();
    if (!midiOutputs.isEmpty()) {
        midiOutput = juce::MidiOutput::openDevice(midiOutputs[0].identifier);
    }
    
    // Initial scan
    scanForMidiFiles();
    
    // Start timer for analysis and playback
    startTimer(50);
    
    setSize(900, 650);
}

MIDIXplorerEditor::~MIDIXplorerEditor()
{
    stopTimer();
    stopPlayback();
}

void MIDIXplorerEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1e1e1e));
    
    // Detail panel background
    auto detailArea = getLocalBounds().reduced(10);
    detailArea.removeFromTop(40); // title
    detailArea.removeFromTop(35); // buttons
    detailArea.removeFromTop(5);
    detailArea = detailArea.removeFromRight(220);
    g.setColour(juce::Colour(0xff252525));
    g.fillRoundedRectangle(detailArea.toFloat(), 8.0f);
    g.setColour(juce::Colour(0xff3a3a3a));
    g.drawRoundedRectangle(detailArea.toFloat(), 8.0f, 1.0f);
}

void MIDIXplorerEditor::resized()
{
    auto area = getLocalBounds().reduced(10);
    
    titleLabel.setBounds(area.removeFromTop(40));
    
    auto topRow = area.removeFromTop(35);
    stopBtn.setBounds(topRow.removeFromRight(70));
    topRow.removeFromRight(5);
    playBtn.setBounds(topRow.removeFromRight(70));
    topRow.removeFromRight(20);
    refreshBtn.setBounds(topRow.removeFromRight(80));
    topRow.removeFromRight(5);
    addFolderBtn.setBounds(topRow.removeFromRight(100));
    
    area.removeFromTop(5);
    
    statusLabel.setBounds(area.removeFromBottom(25));
    
    // Detail panel on right
    auto detailArea = area.removeFromRight(220).reduced(10);
    detailTitle.setBounds(detailArea.removeFromTop(30));
    detailArea.removeFromTop(10);
    detailKey.setBounds(detailArea.removeFromTop(25));
    detailNotes.setBounds(detailArea.removeFromTop(25));
    detailDuration.setBounds(detailArea.removeFromTop(25));
    
    area.removeFromRight(10);
    fileListBox.setBounds(area);
}

void MIDIXplorerEditor::timerCallback()
{
    // Analyze unanalyzed files (one per timer tick to avoid blocking)
    for (size_t i = 0; i < midiFiles.size(); ++i) {
        if (!midiFiles[i].analyzed) {
            analyzeFile(i);
            fileListBox.repaintRow((int)i);
            break; // One file per tick
        }
    }
    
    // Handle MIDI playback
    if (isPlaying && midiOutput != nullptr) {
        double currentTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        double elapsed = currentTime - playbackStartTime;
        
        while (playbackNoteIndex < playbackSequence.getNumEvents()) {
            auto* event = playbackSequence.getEventPointer(playbackNoteIndex);
            if (event->message.getTimeStamp() <= elapsed) {
                midiOutput->sendMessageNow(event->message);
                playbackNoteIndex++;
            } else {
                break;
            }
        }
        
        if (playbackNoteIndex >= playbackSequence.getNumEvents()) {
            stopPlayback();
        }
    }
}

void MIDIXplorerEditor::addFolder()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Select a folder containing MIDI files",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory),
        "", true
    );
    
    auto flags = juce::FileBrowserComponent::openMode | 
                 juce::FileBrowserComponent::canSelectDirectories;
    
    fileChooser->launchAsync(flags, [this](const juce::FileChooser& fc) {
        auto result = fc.getResult();
        if (result.exists() && result.isDirectory()) {
            if (!scanFolders.contains(result.getFullPathName())) {
                scanFolders.add(result.getFullPathName());
            }
            scanForMidiFiles();
        }
    });
}

void MIDIXplorerEditor::scanForMidiFiles()
{
    midiFiles.clear();
    
    for (const auto& folderPath : scanFolders) {
        juce::File folder(folderPath);
        if (folder.isDirectory()) {
            auto files = folder.findChildFiles(juce::File::findFiles, true, "*.mid;*.midi");
            for (const auto& file : files) {
                MIDIFileInfo info;
                info.fileName = file.getFileName();
                info.fullPath = file.getFullPathName();
                info.analyzed = false;
                midiFiles.push_back(info);
            }
        }
    }
    
    fileListBox.updateContent();
    statusLabel.setText(juce::String((int)midiFiles.size()) + " MIDI files found", juce::dontSendNotification);
}

void MIDIXplorerEditor::analyzeFile(size_t index)
{
    if (index >= midiFiles.size()) return;
    
    auto& info = midiFiles[index];
    juce::File file(info.fullPath);
    
    if (!file.existsAsFile()) {
        info.key = "Error";
        info.analyzed = true;
        return;
    }
    
    juce::FileInputStream stream(file);
    if (!stream.openedOk()) {
        info.key = "Error";
        info.analyzed = true;
        return;
    }
    
    juce::MidiFile midiFile;
    if (!midiFile.readFrom(stream)) {
        info.key = "Error";
        info.analyzed = true;
        return;
    }
    
    midiFile.convertTimestampTicksToSeconds();
    
    // Get duration
    double maxTime = 0;
    for (int t = 0; t < midiFile.getNumTracks(); ++t) {
        auto* track = midiFile.getTrack(t);
        if (track && track->getNumEvents() > 0) {
            double trackEnd = track->getEventPointer(track->getNumEvents() - 1)->message.getTimeStamp();
            maxTime = std::max(maxTime, trackEnd);
        }
    }
    info.duration = maxTime;
    
    // Detect key
    detectKeyFromMidi(midiFile, info.key, info.scale, info.noteCount);
    info.analyzed = true;
}

void MIDIXplorerEditor::onFileSelected(int index)
{
    selectedIndex = index;
    updateDetailPanel();
}

void MIDIXplorerEditor::updateDetailPanel()
{
    if (selectedIndex < 0 || selectedIndex >= (int)midiFiles.size()) {
        detailTitle.setText("Select a file", juce::dontSendNotification);
        detailKey.setText("Key: -", juce::dontSendNotification);
        detailNotes.setText("Notes: -", juce::dontSendNotification);
        detailDuration.setText("Duration: -", juce::dontSendNotification);
        return;
    }
    
    auto& info = midiFiles[(size_t)selectedIndex];
    detailTitle.setText(info.fileName, juce::dontSendNotification);
    
    if (info.analyzed) {
        detailKey.setText("Key: " + info.key + " " + info.scale, juce::dontSendNotification);
        detailNotes.setText("Notes: " + juce::String(info.noteCount), juce::dontSendNotification);
        
        int minutes = (int)(info.duration / 60);
        int seconds = (int)(info.duration) % 60;
        detailDuration.setText("Duration: " + juce::String(minutes) + ":" + 
                               juce::String(seconds).paddedLeft('0', 2), juce::dontSendNotification);
    } else {
        detailKey.setText("Key: Analyzing...", juce::dontSendNotification);
        detailNotes.setText("Notes: -", juce::dontSendNotification);
        detailDuration.setText("Duration: -", juce::dontSendNotification);
    }
}

void MIDIXplorerEditor::playSelectedFile()
{
    if (selectedIndex < 0 || selectedIndex >= (int)midiFiles.size()) {
        statusLabel.setText("Select a file to play", juce::dontSendNotification);
        return;
    }
    
    stopPlayback();
    
    auto& info = midiFiles[(size_t)selectedIndex];
    juce::File file(info.fullPath);
    
    juce::FileInputStream stream(file);
    if (!stream.openedOk() || !currentMidiFile.readFrom(stream)) {
        statusLabel.setText("Failed to load MIDI file", juce::dontSendNotification);
        return;
    }
    
    currentMidiFile.convertTimestampTicksToSeconds();
    
    // Merge all tracks into one sequence
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
    
    if (midiOutput == nullptr) {
        // Try to open MIDI output again
        auto devices = juce::MidiOutput::getAvailableDevices();
        if (!devices.isEmpty()) {
            midiOutput = juce::MidiOutput::openDevice(devices[0].identifier);
        }
    }
    
    if (midiOutput == nullptr) {
        statusLabel.setText("No MIDI output available - check Audio/MIDI settings", juce::dontSendNotification);
        return;
    }
    
    isPlaying = true;
    playbackStartTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    playbackNoteIndex = 0;
    
    statusLabel.setText("Playing: " + info.fileName, juce::dontSendNotification);
}

void MIDIXplorerEditor::stopPlayback()
{
    if (isPlaying && midiOutput != nullptr) {
        // Send all notes off
        for (int ch = 1; ch <= 16; ++ch) {
            midiOutput->sendMessageNow(juce::MidiMessage::allNotesOff(ch));
        }
    }
    
    isPlaying = false;
    playbackNoteIndex = 0;
    
    if (selectedIndex >= 0) {
        statusLabel.setText("Stopped", juce::dontSendNotification);
    }
}
