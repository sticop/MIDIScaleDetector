#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <string>
#include <memory>

// Forward declarations
namespace MIDIScaleDetector {
    class Database;
}

// MIDI File item for the list
struct MIDIFileItem {
    std::string name;
    std::string filePath;
    std::string scale;
    int noteCount = 0;
    double duration = 0.0;
};

// Custom list box model for MIDI files
class MIDIFileListModel : public juce::ListBoxModel {
public:
    int getNumRows() override { return static_cast<int>(files.size()); }
    
    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override {
        if (rowNumber < 0 || static_cast<size_t>(rowNumber) >= files.size()) return;
        
        const auto& file = files[static_cast<size_t>(rowNumber)];
        
        // Background
        if (rowIsSelected) {
            g.fillAll(juce::Colour(0xFF3A7BD5));
        } else if (rowNumber % 2 == 0) {
            g.fillAll(juce::Colour(0xFF2A2A2A));
        } else {
            g.fillAll(juce::Colour(0xFF252525));
        }
        
        // File name
        g.setColour(rowIsSelected ? juce::Colours::white : juce::Colour(0xFFE0E0E0));
        g.drawText(file.name, 10, 0, width - 150, height, juce::Justification::centredLeft);
        
        // Scale
        g.setColour(juce::Colour(0xFF8AB4F8));
        g.drawText(file.scale, width - 140, 0, 130, height, juce::Justification::centredRight);
    }
    
    juce::var getDragSourceDescription(const juce::SparseSet<int>& selectedRows) override {
        if (selectedRows.isEmpty()) return {};
        int row = selectedRows[0];
        if (row >= 0 && static_cast<size_t>(row) < files.size()) {
            return juce::var(juce::String(files[static_cast<size_t>(row)].filePath));
        }
        return {};
    }
    
    std::vector<MIDIFileItem> files;
};

// Main Plugin Editor
class MIDIXplorerEditor : public juce::AudioProcessorEditor,
                          public juce::Timer,
                          public juce::DragAndDropContainer,
                          public juce::FileDragAndDropTarget,
                          public juce::KeyListener {
public:
    explicit MIDIXplorerEditor(juce::AudioProcessor& processor);
    ~MIDIXplorerEditor() override;
    
    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    
    // Keyboard handling
    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;
    
    // File drag and drop
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;
    
private:
    juce::AudioProcessor& processorRef;
    
    // Database
    std::unique_ptr<MIDIScaleDetector::Database> database;
    
    // All files (for filtering)
    std::vector<MIDIFileItem> allFiles;
    
    // File chooser (must persist for async)
    std::unique_ptr<juce::FileChooser> fileChooser;
    
    // UI Components
    juce::ListBox fileListBox;
    MIDIFileListModel fileListModel;
    
    // Header/toolbar
    juce::Label titleLabel;
    juce::TextButton addFolderBtn;
    juce::TextButton refreshBtn;
    
    // Search/Filter
    juce::TextEditor searchBox;
    juce::ComboBox scaleFilter;
    
    // Transport
    juce::TextButton playBtn;
    juce::TextButton stopBtn;
    
    // Detail panel
    juce::GroupComponent detailPanel;
    juce::Label detailFileName;
    juce::Label detailScale;
    juce::Label detailDuration;
    juce::Label detailNotes;
    
    // Status
    juce::Label statusLabel;
    juce::Label instructionLabel;
    
    // State
    bool isPlaying = false;
    
    // Methods
    void setupUI();
    void loadFiles();
    void filterFiles();
    void updateDetails();
    void playSelectedFile();
    void stopPlayback();
    void addFolder();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MIDIXplorerEditor)
};
