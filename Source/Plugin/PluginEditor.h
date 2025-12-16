#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

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
    juce::TextButton refreshBtn;
    juce::ListBox fileListBox;
    
    // File chooser must persist for async operation
    std::unique_ptr<juce::FileChooser> fileChooser;
    
    // Folders to scan
    juce::StringArray scanFolders;
    
    // Simple list model
    class SimpleListModel : public juce::ListBoxModel {
    public:
        juce::StringArray items;
        juce::StringArray fullPaths;
        int getNumRows() override { return items.size(); }
        void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) override {
            if (selected) g.fillAll(juce::Colour(0xFF3A7BD5));
            else if (row % 2 == 0) g.fillAll(juce::Colour(0xFF2A2A2A));
            else g.fillAll(juce::Colour(0xFF252525));
            g.setColour(juce::Colours::white);
            if (row < items.size()) g.drawText(items[row], 10, 0, w - 20, h, juce::Justification::left);
        }
        
        juce::var getDragSourceDescription(const juce::SparseSet<int>& selectedRows) override {
            if (selectedRows.isEmpty() || selectedRows[0] >= fullPaths.size()) return {};
            return juce::var(fullPaths[selectedRows[0]]);
        }
    };
    SimpleListModel listModel;
    
    void addFolder();
    void scanForMidiFiles();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MIDIXplorerEditor)
};
