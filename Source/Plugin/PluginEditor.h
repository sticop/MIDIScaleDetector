#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

// Simple Plugin Editor - minimal version to test stability
class MIDIXplorerEditor : public juce::AudioProcessorEditor {
public:
    explicit MIDIXplorerEditor(juce::AudioProcessor& processor);
    ~MIDIXplorerEditor() override = default;
    
    void paint(juce::Graphics&) override;
    void resized() override;
    
private:
    juce::Label titleLabel;
    juce::Label statusLabel;
    juce::TextButton addFolderBtn;
    juce::ListBox fileListBox;
    
    // Simple list model
    class SimpleListModel : public juce::ListBoxModel {
    public:
        juce::StringArray items;
        int getNumRows() override { return items.size(); }
        void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) override {
            if (selected) g.fillAll(juce::Colours::blue);
            g.setColour(juce::Colours::white);
            if (row < items.size()) g.drawText(items[row], 5, 0, w - 10, h, juce::Justification::left);
        }
    };
    SimpleListModel listModel;
    
    void scanForMidiFiles();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MIDIXplorerEditor)
};
