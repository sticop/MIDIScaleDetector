#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>

class MIDIXplorerEditor : public juce::AudioProcessorEditor,
                          public juce::Timer {
public:
    explicit MIDIXplorerEditor(juce::AudioProcessor& processor);
    ~MIDIXplorerEditor() override;
    
    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    
private:
    // File info structure
    struct MIDIFileInfo {
        juce::String fileName;
        juce::String fullPath;
        juce::String key = "Analyzing...";
        juce::String scale = "";
        int noteCount = 0;
        double duration = 0.0;
        bool analyzed = false;
    };
    
    juce::Label titleLabel;
    juce::Label statusLabel;
    juce::TextButton addFolderBtn;
    juce::TextButton refreshBtn;
    juce::TextButton playBtn;
    juce::TextButton stopBtn;
    juce::ListBox fileListBox;
    
    // Detail panel
    juce::Label detailTitle;
    juce::Label detailKey;
    juce::Label detailNotes;
    juce::Label detailDuration;
    
    // File chooser
    std::unique_ptr<juce::FileChooser> fileChooser;
    
    // Folders to scan
    juce::StringArray scanFolders;
    
    // File list
    std::vector<MIDIFileInfo> midiFiles;
    int selectedIndex = -1;
    
    // MIDI playback
    juce::MidiFile currentMidiFile;
    juce::MidiMessageSequence playbackSequence;
    double playbackStartTime = 0;
    int playbackNoteIndex = 0;
    bool isPlaying = false;
    std::unique_ptr<juce::MidiOutput> midiOutput;
    
    // List model
    class FileListModel : public juce::ListBoxModel {
    public:
        MIDIXplorerEditor& owner;
        FileListModel(MIDIXplorerEditor& o) : owner(o) {}
        
        int getNumRows() override { return (int)owner.midiFiles.size(); }
        
        void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) override {
            if (row < 0 || row >= (int)owner.midiFiles.size()) return;
            
            auto& file = owner.midiFiles[(size_t)row];
            
            // Background
            if (selected) g.fillAll(juce::Colour(0xFF3A7BD5));
            else if (row % 2 == 0) g.fillAll(juce::Colour(0xFF2A2A2A));
            else g.fillAll(juce::Colour(0xFF252525));
            
            // File name
            g.setColour(juce::Colours::white);
            g.drawText(file.fileName, 10, 0, w - 200, h, juce::Justification::left);
            
            // Key/Scale
            if (file.analyzed) {
                g.setColour(juce::Colour(0xFF8AB4F8));
                g.drawText(file.key + " " + file.scale, w - 180, 0, 100, h, juce::Justification::centred);
                
                // Status indicator
                g.setColour(juce::Colours::green);
                g.fillEllipse((float)(w - 70), (float)(h/2 - 5), 10.0f, 10.0f);
            } else {
                g.setColour(juce::Colours::grey);
                g.drawText("Analyzing...", w - 180, 0, 100, h, juce::Justification::centred);
                
                // Status indicator
                g.setColour(juce::Colours::orange);
                g.fillEllipse((float)(w - 70), (float)(h/2 - 5), 10.0f, 10.0f);
            }
        }
        
        void selectedRowsChanged(int lastRowSelected) override {
            owner.onFileSelected(lastRowSelected);
        }
    };
    std::unique_ptr<FileListModel> listModel;
    
    void addFolder();
    void scanForMidiFiles();
    void analyzeFile(size_t index);
    void onFileSelected(int index);
    void playSelectedFile();
    void stopPlayback();
    void updateDetailPanel();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MIDIXplorerEditor)
};
