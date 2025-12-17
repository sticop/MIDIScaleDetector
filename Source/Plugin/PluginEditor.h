#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_formats/juce_audio_formats.h>

namespace MIDIScaleDetector {
    class MIDIScalePlugin;
}

class MIDIXplorerEditor : public juce::AudioProcessorEditor, 
                          private juce::Timer,
                          public juce::DragAndDropContainer {
public:
    explicit MIDIXplorerEditor(juce::AudioProcessor& p);
    ~MIDIXplorerEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;
    bool keyPressed(const juce::KeyPress& key) override;

    struct Library {
        juce::String name;
        juce::String path;
        bool enabled = true;
        int fileCount = 0;
        bool isScanning = false;
    };

    struct MIDIFileInfo {
        juce::String fileName;
        juce::String fullPath;
        juce::String key;
        juce::String libraryName;
        bool analyzed = false;
    };

private:
    class LibraryListModel : public juce::ListBoxModel {
    public:
        explicit LibraryListModel(MIDIXplorerEditor& o) : owner(o) {}
        int getNumRows() override { return (int)owner.libraries.size(); }
        void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) override;
        void listBoxItemClicked(int row, const juce::MouseEvent& e) override;
    private:
        MIDIXplorerEditor& owner;
    };

    class FileListModel : public juce::ListBoxModel {
    public:
        explicit FileListModel(MIDIXplorerEditor& o) : owner(o) {}
        int getNumRows() override { return (int)owner.filteredFiles.size(); }
        void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) override;
        void selectedRowsChanged(int lastRowSelected) override;
        juce::var getDragSourceDescription(const juce::SparseSet<int>& selectedRows) override;
        void listBoxItemClicked(int row, const juce::MouseEvent& e) override;
    private:
        MIDIXplorerEditor& owner;
    };

    // Custom ListBox for external drag and drop
    class DraggableListBox : public juce::ListBox {
    public:
        DraggableListBox(const juce::String& name, MIDIXplorerEditor& o) 
            : juce::ListBox(name), owner(o) {}
        
        void mouseDrag(const juce::MouseEvent& e) override {
            if (e.getDistanceFromDragStart() > 5 && !isDragging) {
                int row = getRowContainingPosition(e.x, e.y);
                if (row >= 0 && row < (int)owner.filteredFiles.size()) {
                    auto file = juce::File(owner.filteredFiles[(size_t)row].fullPath);
                    if (file.existsAsFile()) {
                        isDragging = true;
                        juce::StringArray files;
                        files.add(file.getFullPathName());
                        juce::DragAndDropContainer::performExternalDragDropOfFiles(files, false);
                        isDragging = false;
                    }
                }
            }
            juce::ListBox::mouseDrag(e);
        }
    private:
        MIDIXplorerEditor& owner;
        bool isDragging = false;
    };

    std::unique_ptr<LibraryListModel> libraryModel;
    std::unique_ptr<FileListModel> fileModel;

    juce::TextButton addLibraryBtn{"+ Add Library"};
    juce::Label librariesLabel;
    juce::ListBox libraryListBox{"Libraries"};
    
    juce::Label fileCountLabel;
    juce::ComboBox keyFilterCombo;
    juce::TextEditor searchBox;
    std::unique_ptr<DraggableListBox> fileListBox;
    
    juce::ToggleButton previewToggle{"Preview"};
    juce::ToggleButton syncToHostToggle{"Sync"};
    juce::Slider transportSlider;
    
    std::vector<Library> libraries;
    std::vector<MIDIFileInfo> allFiles;
    std::vector<MIDIFileInfo> filteredFiles;
    juce::StringArray detectedKeys;
    
    int selectedFileIndex = -1;
    bool isPlaying = false;
    double playbackStartTime = 0;
    double playbackStartBeat = 0;   // Beat position when playback started
    int playbackNoteIndex = 0;
    
    // Host sync state
    double lastHostBpm = 120.0;
    double midiFileBpm = 120.0;
    bool wasHostPlaying = false;    // Track host play state changes
    double lastBeatsElapsed = 0;    // For detecting transport jumps
    
    juce::MidiFile currentMidiFile;
    juce::MidiMessageSequence playbackSequence;
    std::unique_ptr<juce::FileChooser> fileChooser;
    
    MIDIScaleDetector::MIDIScalePlugin* pluginProcessor = nullptr;
    
    void addLibrary();
    void scanLibraries();
    void scanLibrary(size_t index);
    void analyzeFile(size_t index);
    void filterFiles();
    void updateKeyFilterFromDetectedScales();
    void playSelectedFile();
    void stopPlayback();
    void revealInFinder(const juce::String& path);
    void selectAndPreview(int row);
    
    // Tempo helpers
    double getHostBpm();
    double getHostBeatPosition();
    bool isHostPlaying();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MIDIXplorerEditor)
};
