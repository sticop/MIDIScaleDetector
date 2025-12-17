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
        juce::String relativeKey;  // Circle of Fifths relative major/minor
        juce::String libraryName;
        double duration = 0.0;  // Duration in seconds
        double bpm = 120.0;     // Tempo in BPM
        juce::String instrument = "---";  // GM instrument name
        bool analyzed = false;
        bool favorite = false;  // Marked as favorite
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

    class DraggableListBox : public juce::ListBox {
    public:
        DraggableListBox(const juce::String& name, MIDIXplorerEditor& o)
            : juce::ListBox(name), owner(o) {}

        void mouseDown(const juce::MouseEvent& e) override {
            dragStartRow = getRowContainingPosition(e.x, e.y);
            
            // Check if clicking on heart area (first 24 pixels of the row)
            if (e.x < 24 && dragStartRow >= 0 && dragStartRow < (int)owner.filteredFiles.size()) {
                owner.toggleFavorite(dragStartRow);
                return;  // Don't process further
            }
            
            juce::ListBox::mouseDown(e);
        }

        void mouseDrag(const juce::MouseEvent& e) override {
            if (e.getDistanceFromDragStart() > 10 && !isDragging && dragStartRow >= 0) {
                if (dragStartRow < (int)owner.filteredFiles.size()) {
                    isDragging = true;
                    auto filePath = owner.filteredFiles[(size_t)dragStartRow].fullPath;
                    juce::File file(filePath);
                    if (file.existsAsFile()) {
                        juce::StringArray files;
                        files.add(file.getFullPathName());
                        // Use copy mode for DAW compatibility
                        juce::DragAndDropContainer::performExternalDragDropOfFiles(files, true);
                    }
                    isDragging = false;
                }
            }
        }

        void mouseUp(const juce::MouseEvent& e) override {
            isDragging = false;
            dragStartRow = -1;
            juce::ListBox::mouseUp(e);
        }
    private:
        MIDIXplorerEditor& owner;
        bool isDragging = false;
        int dragStartRow = -1;
    };

    // MIDI Note Viewer (Piano Roll)
    class MIDINoteViewer : public juce::Component {
    public:
        MIDINoteViewer() {}
        void paint(juce::Graphics& g) override;
        void setSequence(const juce::MidiMessageSequence* seq, double duration);
        void setPlaybackPosition(double position);
    private:
        const juce::MidiMessageSequence* sequence = nullptr;
        double totalDuration = 1.0;
        double playPosition = 0.0;
        int lowestNote = 127;
        int highestNote = 0;
    };

    std::unique_ptr<LibraryListModel> libraryModel;
    std::unique_ptr<FileListModel> fileModel;

    juce::TextButton addLibraryBtn{"+ Add Library"};
    juce::Label librariesLabel;
    juce::ListBox libraryListBox{"Libraries"};

    juce::Label fileCountLabel;
    juce::ComboBox keyFilterCombo;
    juce::ComboBox sortCombo;
    juce::ComboBox quantizeCombo;
    juce::TextButton quantizeButton{"Quantize"};
    juce::TextEditor searchBox;
    std::unique_ptr<DraggableListBox> fileListBox;

    juce::TextButton playPauseButton;
    bool isPlaying = true;
    juce::ToggleButton syncToHostToggle{"DAW Sync"};
    juce::Slider transportSlider;
    juce::Label timeDisplayLabel;  // Shows elapsed / total time
    MIDINoteViewer midiNoteViewer;

    std::vector<Library> libraries;
    std::vector<MIDIFileInfo> allFiles;
    std::vector<MIDIFileInfo> filteredFiles;
    juce::StringArray detectedKeys;

    int selectedFileIndex = -1;
    juce::String pendingSelectedFilePath;
    bool fileLoaded = false;
    double playbackStartTime = 0;
    double playbackStartBeat = 0;
    int playbackNoteIndex = 0;

    // Host sync state
    double lastHostBpm = 120.0;
    double midiFileBpm = 120.0;
    double midiFileDuration = 0.0;  // Actual duration of the MIDI file in seconds
    double currentPlaybackPosition = 0.0;  // 0.0 to 1.0 for progress display
    bool wasHostPlaying = false;
    double lastBeatPosition = 0;

    // Pending file change
    bool pendingFileChange = false;
    int pendingFileIndex = -1;

    juce::MidiFile currentMidiFile;
    juce::MidiMessageSequence playbackSequence;
    std::unique_ptr<juce::FileChooser> fileChooser;

    MIDIScaleDetector::MIDIScalePlugin* pluginProcessor = nullptr;

    void addLibrary();
    void scanLibraries();
    void scanLibrary(size_t index);
    void refreshLibrary(size_t index);
    void analyzeFile(size_t index);
    void filterFiles();
    void sortFiles();
    void updateKeyFilterFromDetectedScales();
    void loadSelectedFile();
    void scheduleFileChange();
    void stopPlayback();
    void restartPlayback();
    void revealInFinder(const juce::String& path);
    void toggleFavorite(int row);
    void saveFavorites();
    void loadFavorites();
    void quantizeMidi();
    void selectAndPreview(int row);

    // Persistence
    void saveLibraries();
    void loadLibraries();
    juce::File getSettingsFile();

    double getHostBpm();
    double getHostBeatPosition();
    bool isHostPlaying();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MIDIXplorerEditor)
};

