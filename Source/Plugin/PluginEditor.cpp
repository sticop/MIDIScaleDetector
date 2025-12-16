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
    statusLabel.setText("Scanning for MIDI files...", juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(statusLabel);
    
    // Add folder button
    addFolderBtn.setButtonText("Add Folder");
    addFolderBtn.onClick = [this]() {
        // Simple folder addition - just rescan
        scanForMidiFiles();
    };
    addAndMakeVisible(addFolderBtn);
    
    // File list
    fileListBox.setModel(&listModel);
    fileListBox.setRowHeight(24);
    fileListBox.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff2a2a2a));
    addAndMakeVisible(fileListBox);
    
    // Scan for files
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
    addFolderBtn.setBounds(topRow.removeFromRight(100));
    
    area.removeFromTop(10);
    
    statusLabel.setBounds(area.removeFromBottom(25));
    
    fileListBox.setBounds(area);
}

void MIDIXplorerEditor::scanForMidiFiles()
{
    listModel.items.clear();
    
    // Scan common locations
    juce::Array<juce::File> searchPaths;
    searchPaths.add(juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile("Music"));
    searchPaths.add(juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile("Downloads"));
    
    for (const auto& path : searchPaths)
    {
        if (path.isDirectory())
        {
            auto files = path.findChildFiles(juce::File::findFiles, true, "*.mid;*.midi");
            for (const auto& file : files)
            {
                listModel.items.add(file.getFileName());
            }
        }
    }
    
    fileListBox.updateContent();
    statusLabel.setText(juce::String(listModel.items.size()) + " MIDI files found", juce::dontSendNotification);
}
