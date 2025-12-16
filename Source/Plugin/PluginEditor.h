#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_formats/juce_audio_formats.h>

// Forward declaration
namespace MIDIScaleDetector {
    class MIDIScalePlugin;
}

class MIDIXplorerEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit MIDIXplorerEditor(juce::AudioProcessor& p);
    ~MIDIXplorerEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

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
    private:
        MIDIXplorerEditor& owner;
    };

    std::unique_ptr<LibraryListModel> libraryModel;
    std::unique_ptr<FileListModel> fileModel;

    juce::TextButton addLibraryBtn{"+ Add Library"};
    juce::Label librariesLabel;
    juce::ListBox libraryListBox{"Libraries"};
    
    juce::Label fileCountLabel;
    juce::ComboBox keyFilterCombo;
    juce::TextEditor searchBox;
    juce::ListBox fileListBox{"Files"};
    
    juce::TextButton playBtn, stopBtn;
    juce::Slider transportSlider;
    
    std::vector<Library> libraries;
    std::vector<MIDIFileInfo> allFiles;
    std::vector<MIDIFileInfo> filteredFiles;
    
    int selectedFileIndex = -1;
    bool isPlaying = false;
    double playbackStartTime = 0;
    int playbackNoteIndex = 0;
    
    juce::MidiFile currentMidiFile;
    juce::MidiMessageSequence playbackSequence;
    std::unique_ptr<juce::FileChooser> fileChooser;
    
    // Reference to our plugin processor
    MIDIScaleDetector::MIDIScalePlugin* pluginProcessor = nullptr;
    
    void addLibrary();
    void scanLibraries();
    void scanLibrary(size_t index);
    void analyzeFile(size_t index);
    void filterFiles();
    void populateKeyFilter();
    void playSelectedFile();
    void stopPlayback();
    void revealInFinder(const juce::String& path);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MIDIXplorerEditor)
};
