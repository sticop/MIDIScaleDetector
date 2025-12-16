#include "PluginEditor.h"
#include "Core/Database/Database.h"
#include "Core/MIDIParser/MIDIParser.h"
#include "Core/ScaleDetector/ScaleDetector.h"

MIDIXplorerEditor::MIDIXplorerEditor(juce::AudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    // Initialize database
    auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("MIDIXplorer");
    appData.createDirectory();
    std::string dbPath = appData.getChildFile("midi_database.db").getFullPathName().toStdString();
    
    database = std::make_unique<MIDIScaleDetector::Database>();
    database->initialize(dbPath);
    
    // Register key listener
    addKeyListener(this);
    setWantsKeyboardFocus(true);
    
    setupUI();
    loadFiles();
    startTimer(100);
    
    setSize(900, 700);
}

MIDIXplorerEditor::~MIDIXplorerEditor()
{
    removeKeyListener(this);
    stopTimer();
}

void MIDIXplorerEditor::setupUI()
{
    // Title
    titleLabel.setText("MIDI Xplorer", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(24.0f).withStyle("Bold"));
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);
    
    // Search
    searchBox.setTextToShowWhenEmpty("Search MIDI files...", juce::Colours::grey);
    searchBox.onTextChange = [this]() { filterFiles(); };
    addAndMakeVisible(searchBox);
    
    // Scale filter
    scaleFilter.addItem("All Scales", 1);
    scaleFilter.addItem("C Major", 2);
    scaleFilter.addItem("C Minor", 3);
    scaleFilter.addItem("D Major", 4);
    scaleFilter.addItem("D Minor", 5);
    scaleFilter.addItem("E Major", 6);
    scaleFilter.addItem("E Minor", 7);
    scaleFilter.addItem("F Major", 8);
    scaleFilter.addItem("F Minor", 9);
    scaleFilter.addItem("G Major", 10);
    scaleFilter.addItem("G Minor", 11);
    scaleFilter.addItem("A Major", 12);
    scaleFilter.addItem("A Minor", 13);
    scaleFilter.addItem("B Major", 14);
    scaleFilter.addItem("B Minor", 15);
    scaleFilter.setSelectedId(1);
    scaleFilter.onChange = [this]() { filterFiles(); };
    addAndMakeVisible(scaleFilter);
    
    // Add folder button
    addFolderBtn.setButtonText("Add Folder");
    addFolderBtn.onClick = [this]() { addFolder(); };
    addAndMakeVisible(addFolderBtn);
    
    // Refresh button
    refreshBtn.setButtonText("Refresh");
    refreshBtn.onClick = [this]() { loadFiles(); };
    addAndMakeVisible(refreshBtn);
    
    // File list
    fileListBox.setModel(&fileListModel);
    fileListBox.setRowHeight(24);
    fileListBox.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff2a2a2a));
    addAndMakeVisible(fileListBox);
    
    // Status label
    statusLabel.setText("Ready", juce::dontSendNotification);
    statusLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(statusLabel);
    
    // Detail panel
    detailPanel.setColour(juce::GroupComponent::outlineColourId, juce::Colours::grey);
    detailPanel.setText("File Details");
    addAndMakeVisible(detailPanel);
    
    detailFileName.setText("No file selected", juce::dontSendNotification);
    detailFileName.setFont(juce::FontOptions(16.0f).withStyle("Bold"));
    addAndMakeVisible(detailFileName);
    
    detailScale.setText("Scale: -", juce::dontSendNotification);
    addAndMakeVisible(detailScale);
    
    detailDuration.setText("Duration: -", juce::dontSendNotification);
    addAndMakeVisible(detailDuration);
    
    detailNotes.setText("Notes: -", juce::dontSendNotification);
    addAndMakeVisible(detailNotes);
    
    // Transport
    playBtn.setButtonText("Play");
    playBtn.onClick = [this]() { playSelectedFile(); };
    addAndMakeVisible(playBtn);
    
    stopBtn.setButtonText("Stop");
    stopBtn.onClick = [this]() { stopPlayback(); };
    addAndMakeVisible(stopBtn);
    
    // Instructions
    instructionLabel.setText("Arrow keys: Navigate | Enter: Play | Space: Stop | Drag to DAW",
                             juce::dontSendNotification);
    instructionLabel.setJustificationType(juce::Justification::centred);
    instructionLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(instructionLabel);
}

void MIDIXplorerEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1e1e1e));
}

void MIDIXplorerEditor::resized()
{
    auto area = getLocalBounds().reduced(10);
    
    // Title
    titleLabel.setBounds(area.removeFromTop(40));
    area.removeFromTop(5);
    
    // Search row
    auto searchRow = area.removeFromTop(30);
    searchBox.setBounds(searchRow.removeFromLeft(300));
    searchRow.removeFromLeft(10);
    scaleFilter.setBounds(searchRow.removeFromLeft(150));
    searchRow.removeFromLeft(10);
    addFolderBtn.setBounds(searchRow.removeFromLeft(100));
    searchRow.removeFromLeft(10);
    refreshBtn.setBounds(searchRow.removeFromLeft(80));
    
    area.removeFromTop(10);
    
    // Main content
    auto mainArea = area.removeFromTop(area.getHeight() - 60);
    auto detailWidth = 250;
    
    // File list
    fileListBox.setBounds(mainArea.removeFromLeft(mainArea.getWidth() - detailWidth - 10));
    mainArea.removeFromLeft(10);
    
    // Detail panel
    detailPanel.setBounds(mainArea);
    auto detailArea = mainArea.reduced(15, 25);
    detailFileName.setBounds(detailArea.removeFromTop(25));
    detailArea.removeFromTop(10);
    detailScale.setBounds(detailArea.removeFromTop(20));
    detailArea.removeFromTop(5);
    detailDuration.setBounds(detailArea.removeFromTop(20));
    detailArea.removeFromTop(5);
    detailNotes.setBounds(detailArea.removeFromTop(20));
    detailArea.removeFromTop(20);
    
    auto transportRow = detailArea.removeFromTop(30);
    playBtn.setBounds(transportRow.removeFromLeft(60));
    transportRow.removeFromLeft(10);
    stopBtn.setBounds(transportRow.removeFromLeft(60));
    
    area.removeFromTop(10);
    
    // Status bar
    auto statusRow = area.removeFromTop(25);
    statusLabel.setBounds(statusRow.removeFromLeft(200));
    instructionLabel.setBounds(statusRow);
}

// Helper to parse MIDI file and get info
static void parseMIDIFileInfo(const std::string& filePath, MIDIFileItem& item)
{
    try {
        MIDIScaleDetector::MIDIParser parser;
        MIDIScaleDetector::MIDIFile midiFile;
        
        if (parser.parse(filePath, midiFile)) {
            MIDIScaleDetector::ScaleDetector detector;
            auto result = detector.analyze(midiFile);
            item.scale = result.primaryScale.getName();
            item.noteCount = result.totalNotes;
            item.duration = midiFile.getDuration();
        } else {
            item.scale = "Unknown";
            item.noteCount = 0;
            item.duration = 0.0;
        }
    } catch (...) {
        item.scale = "Unknown";
        item.noteCount = 0;
        item.duration = 0.0;
    }
}

void MIDIXplorerEditor::loadFiles()
{
    fileListModel.files.clear();
    
    // Scan common MIDI locations
    juce::Array<juce::File> searchPaths;
    searchPaths.add(juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile("Music"));
    searchPaths.add(juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile("Documents"));
    searchPaths.add(juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile("Downloads"));
    
    for (const auto& path : searchPaths)
    {
        if (path.isDirectory())
        {
            auto results = path.findChildFiles(juce::File::findFiles, true, "*.mid;*.midi");
            for (const auto& file : results)
            {
                MIDIFileItem item;
                item.name = file.getFileName().toStdString();
                item.filePath = file.getFullPathName().toStdString();
                
                parseMIDIFileInfo(item.filePath, item);
                
                fileListModel.files.push_back(item);
            }
        }
    }
    
    allFiles = fileListModel.files;
    fileListBox.updateContent();
    statusLabel.setText(juce::String(static_cast<int>(fileListModel.files.size())) + " files found", 
                        juce::dontSendNotification);
}

void MIDIXplorerEditor::filterFiles()
{
    juce::String searchText = searchBox.getText().toLowerCase();
    int scaleId = scaleFilter.getSelectedId();
    
    fileListModel.files.clear();
    
    for (const auto& file : allFiles)
    {
        bool matchesSearch = searchText.isEmpty() || 
            juce::String(file.name).toLowerCase().contains(searchText);
        
        bool matchesScale = (scaleId == 1); // All scales
        if (!matchesScale && scaleId >= 2)
        {
            juce::String scaleName = scaleFilter.getItemText(scaleId - 1);
            matchesScale = juce::String(file.scale).containsIgnoreCase(scaleName);
        }
        
        if (matchesSearch && matchesScale)
            fileListModel.files.push_back(file);
    }
    
    fileListBox.updateContent();
    statusLabel.setText(juce::String(static_cast<int>(fileListModel.files.size())) + " files shown",
                        juce::dontSendNotification);
}

void MIDIXplorerEditor::updateDetails()
{
    int selected = fileListBox.getSelectedRow();
    if (selected >= 0 && static_cast<size_t>(selected) < fileListModel.files.size())
    {
        const auto& file = fileListModel.files[static_cast<size_t>(selected)];
        detailFileName.setText(file.name, juce::dontSendNotification);
        detailScale.setText("Scale: " + file.scale, juce::dontSendNotification);
        detailDuration.setText("Duration: " + juce::String(file.duration, 1) + "s", juce::dontSendNotification);
        detailNotes.setText("Notes: " + juce::String(file.noteCount), juce::dontSendNotification);
    }
    else
    {
        detailFileName.setText("No file selected", juce::dontSendNotification);
        detailScale.setText("Scale: -", juce::dontSendNotification);
        detailDuration.setText("Duration: -", juce::dontSendNotification);
        detailNotes.setText("Notes: -", juce::dontSendNotification);
    }
}

void MIDIXplorerEditor::playSelectedFile()
{
    int selected = fileListBox.getSelectedRow();
    if (selected >= 0 && static_cast<size_t>(selected) < fileListModel.files.size())
    {
        isPlaying = true;
        statusLabel.setText("Playing: " + fileListModel.files[static_cast<size_t>(selected)].name, 
                            juce::dontSendNotification);
    }
}

void MIDIXplorerEditor::stopPlayback()
{
    isPlaying = false;
    statusLabel.setText("Stopped", juce::dontSendNotification);
}

void MIDIXplorerEditor::addFolder()
{
    fileChooser = std::make_unique<juce::FileChooser>("Select MIDI Folder",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory));
    
    auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories;
    
    fileChooser->launchAsync(flags, [this](const juce::FileChooser& fc) {
        auto result = fc.getResult();
        if (result.exists() && result.isDirectory())
        {
            auto files = result.findChildFiles(juce::File::findFiles, true, "*.mid;*.midi");
            for (const auto& file : files)
            {
                MIDIFileItem item;
                item.name = file.getFileName().toStdString();
                item.filePath = file.getFullPathName().toStdString();
                
                parseMIDIFileInfo(item.filePath, item);
                
                allFiles.push_back(item);
                fileListModel.files.push_back(item);
            }
            
            fileListBox.updateContent();
            statusLabel.setText(juce::String(static_cast<int>(fileListModel.files.size())) + " files found",
                                juce::dontSendNotification);
        }
    });
}

bool MIDIXplorerEditor::keyPressed(const juce::KeyPress& key, juce::Component*)
{
    int currentRow = fileListBox.getSelectedRow();
    int numRows = static_cast<int>(fileListModel.files.size());
    
    if (key == juce::KeyPress::upKey && currentRow > 0)
    {
        fileListBox.selectRow(currentRow - 1);
        updateDetails();
        return true;
    }
    else if (key == juce::KeyPress::downKey && currentRow < numRows - 1)
    {
        fileListBox.selectRow(currentRow + 1);
        updateDetails();
        return true;
    }
    else if (key == juce::KeyPress::returnKey)
    {
        playSelectedFile();
        return true;
    }
    else if (key == juce::KeyPress::spaceKey)
    {
        if (isPlaying)
            stopPlayback();
        else
            playSelectedFile();
        return true;
    }
    
    return false;
}

void MIDIXplorerEditor::timerCallback()
{
    static int lastSelected = -1;
    int selected = fileListBox.getSelectedRow();
    if (selected != lastSelected)
    {
        lastSelected = selected;
        updateDetails();
    }
}

bool MIDIXplorerEditor::isInterestedInFileDrag(const juce::StringArray&)
{
    return true;
}

void MIDIXplorerEditor::filesDropped(const juce::StringArray& files, int, int)
{
    for (const auto& filePath : files)
    {
        juce::File file(filePath);
        if (file.hasFileExtension("mid") || file.hasFileExtension("midi"))
        {
            MIDIFileItem item;
            item.name = file.getFileName().toStdString();
            item.filePath = file.getFullPathName().toStdString();
            
            parseMIDIFileInfo(item.filePath, item);
            
            allFiles.push_back(item);
            fileListModel.files.push_back(item);
        }
    }
    
    fileListBox.updateContent();
    statusLabel.setText(juce::String(static_cast<int>(fileListModel.files.size())) + " files", 
                        juce::dontSendNotification);
}
