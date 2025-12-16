#include "PluginEditor.h"

MIDIXplorerEditor::MIDIXplorerEditor(juce::AudioProcessor& p)
    : AudioProcessorEditor(&p)
{
    // Title
    titleLabel.setText("MIDI Xplorer", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(24.0f));
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);
    
    // Status
    statusLabel.setText("Click 'Add Folder' to scan for MIDI files", juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(statusLabel);
    
    // Add folder button
    addFolderBtn.setButtonText("Add Folder");
    addFolderBtn.onClick = [this]() { addFolder(); };
    addAndMakeVisible(addFolderBtn);
    
    // Refresh button
    refreshBtn.setButtonText("Refresh");
    refreshBtn.onClick = [this]() { scanForMidiFiles(); };
    addAndMakeVisible(refreshBtn);
    
    // File list
    fileListBox.setModel(&listModel);
    fileListBox.setRowHeight(24);
    fileListBox.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff2a2a2a));
    addAndMakeVisible(fileListBox);
    
    // Add default folders
    auto home = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
    scanFolders.add(home.getChildFile("Music").getFullPathName());
    scanFolders.add(home.getChildFile("Downloads").getFullPathName());
    
    // Initial scan
    scanForMidiFiles();
    
    setSize(800, 600);
}

void MIDIXplorerEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1e1e1e));
}

void MIDIXplorerEditor::resized()
{
    auto area = getLocalBounds().reduced(10);
    
    titleLabel.setBounds(area.removeFromTop(40));
    
    auto topRow = area.removeFromTop(30);
    refreshBtn.setBounds(topRow.removeFromRight(80));
    topRow.removeFromRight(10);
    addFolderBtn.setBounds(topRow.removeFromRight(100));
    
    area.removeFromTop(10);
    
    statusLabel.setBounds(area.removeFromBottom(25));
    
    fileListBox.setBounds(area);
}

void MIDIXplorerEditor::addFolder()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Select a folder containing MIDI files",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory),
        "",
        true
    );
    
    auto folderChooserFlags = juce::FileBrowserComponent::openMode | 
                               juce::FileBrowserComponent::canSelectDirectories;
    
    fileChooser->launchAsync(folderChooserFlags, [this](const juce::FileChooser& fc)
    {
        auto result = fc.getResult();
        if (result.exists() && result.isDirectory())
        {
            juce::String folderPath = result.getFullPathName();
            
            // Add if not already in list
            if (!scanFolders.contains(folderPath))
            {
                scanFolders.add(folderPath);
                statusLabel.setText("Added: " + result.getFileName(), juce::dontSendNotification);
            }
            
            // Rescan all folders
            scanForMidiFiles();
        }
    });
}

void MIDIXplorerEditor::scanForMidiFiles()
{
    listModel.items.clear();
    listModel.fullPaths.clear();
    
    for (const auto& folderPath : scanFolders)
    {
        juce::File folder(folderPath);
        if (folder.isDirectory())
        {
            auto files = folder.findChildFiles(juce::File::findFiles, true, "*.mid;*.midi");
            for (const auto& file : files)
            {
                listModel.items.add(file.getFileName());
                listModel.fullPaths.add(file.getFullPathName());
            }
        }
    }
    
    fileListBox.updateContent();
    statusLabel.setText(juce::String(listModel.items.size()) + " MIDI files found", juce::dontSendNotification);
}
