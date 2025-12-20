#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Standalone/LicenseManager.h"

namespace MIDIScaleDetector {
    class MIDIScalePlugin;
}

class MIDIXplorerEditor : public juce::AudioProcessorEditor,
                          private juce::Timer,
                          public juce::DragAndDropContainer,
                          public LicenseManager::Listener {
public:
    explicit MIDIXplorerEditor(juce::AudioProcessor& p);
    ~MIDIXplorerEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;
    bool keyPressed(const juce::KeyPress& key) override;

    // LicenseManager::Listener
    void licenseStatusChanged(LicenseManager::LicenseStatus status, const LicenseManager::LicenseInfo& info) override;

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
        double duration = 0.0;      // Duration in seconds (rounded to bars)
        double durationBeats = 0.0; // Duration in beats
        double bpm = 120.0;     // Tempo in BPM
        juce::int64 fileSize = 0;  // File size in bytes
        juce::String instrument = "---";  // GM instrument name
        bool analyzed = false;
        bool isAnalyzing = false;  // Currently being analyzed
        bool favorite = false;  // Marked as favorite
    };

private:
    class LibraryListModel : public juce::ListBoxModel {
    public:
        explicit LibraryListModel(MIDIXplorerEditor& o) : owner(o) {}
        int getNumRows() override { return 3 + (int)owner.libraries.size(); }  // All, Favorites, Recently Played + user libraries
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
            : juce::ListBox(name), owner(o) {
            setMultipleSelectionEnabled(false);
        }

        void mouseDown(const juce::MouseEvent& e) override {
            dragStartRow = getRowContainingPosition(e.x, e.y);
            dragStartPos = e.getPosition();
            isDragging = false;

            // Check if clicking on star area (first 24 pixels of the row)
            if (e.x < 24 && dragStartRow >= 0 && dragStartRow < (int)owner.filteredFiles.size()) {
                owner.toggleFavorite(dragStartRow);
                starClicked = true;
                return;
            }
            starClicked = false;

            juce::ListBox::mouseDown(e);
        }

        void mouseDrag(const juce::MouseEvent& e) override {
            if (starClicked) return;
            if (isDragging) return;

            // Start native file drag when mouse moves enough
            if (dragStartRow >= 0 && dragStartRow < (int)owner.filteredFiles.size()) {
                auto distance = e.getPosition().getDistanceFrom(dragStartPos);
                if (distance > 5) {
                    auto filePath = owner.filteredFiles[(size_t)dragStartRow].fullPath;
                    juce::File srcFile(filePath);
                    if (srcFile.existsAsFile()) {
                        isDragging = true;

                        // Copy to temp directory for reliable drag to DAW
                        auto dragFile = makeTempCopyForDrag(srcFile);
                        if (dragFile.existsAsFile()) {
                            juce::StringArray files;
                            files.add(dragFile.getFullPathName());
                            juce::DragAndDropContainer::performExternalDragDropOfFiles(files, false);
                        }

                        isDragging = false;
                        dragStartRow = -1;
                    }
                }
            }
        }

        void mouseUp(const juce::MouseEvent& e) override {
            isDragging = false;
            dragStartRow = -1;
            starClicked = false;
            juce::ListBox::mouseUp(e);
        }

    private:
        // Create a temp copy of the file for reliable drag to DAW
        juce::File makeTempCopyForDrag(const juce::File& src) {
            auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                             .getChildFile("MIDIXplorerDrags");
            tempDir.createDirectory();

            auto dst = tempDir.getNonexistentChildFile(src.getFileNameWithoutExtension(),
                                                        src.getFileExtension());

            if (src.copyFileTo(dst))
                return dst;

            return {};
        }

        MIDIXplorerEditor& owner;
        bool isDragging = false;
        bool starClicked = false;
        int dragStartRow = -1;
        juce::Point<int> dragStartPos;
    };

    // MIDI Note Viewer (Piano Roll)
    class MIDINoteViewer : public juce::Component {
    public:
        void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
        void mouseMagnify(const juce::MouseEvent& event, float scaleFactor) override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;
        void mouseUp(const juce::MouseEvent& e) override;
        float getZoomLevel() const { return zoomLevel; }
        void setZoomLevel(float newZoom) { zoomLevel = juce::jlimit(0.5f, 32.0f, newZoom); repaint(); }
        void resetZoom() { zoomLevel = 1.0f; scrollOffset = 0.0f; repaint(); }
        void setPlayingNotes(const std::vector<int>& notes) { playingNotes = notes; repaint(); }
        static constexpr int PIANO_WIDTH = 40;  // Width of piano keyboard on left
    public:
        MIDINoteViewer() { setMouseCursor(juce::MouseCursor::CrosshairCursor); }
        void paint(juce::Graphics& g) override;
        void setSequence(const juce::MidiMessageSequence* seq, double duration);
        void setPlaybackPosition(double position);
        void mouseMove(const juce::MouseEvent& e) override;
        void mouseExit(const juce::MouseEvent& e) override;

        static juce::String getNoteNameFromMidi(int midiNote);
        static juce::String detectChordName(const std::vector<int>& notes);
    private:
        const juce::MidiMessageSequence* sequence = nullptr;
        double totalDuration = 1.0;
        double playPosition = 0.0;
        int lowestNote = 127;
        int highestNote = 0;
        int hoveredNote = -1;
        juce::Point<int> hoverPos;
        float zoomLevel = 1.0f;
        float scrollOffset = 0.0f;
        std::vector<int> playingNotes;  // Currently playing notes

        // Selection for click-to-zoom
        bool isDraggingSelection = false;
        juce::Point<int> selectionStart;
        juce::Point<int> selectionEnd;
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
    juce::TextEditor searchBox;
    std::unique_ptr<DraggableListBox> fileListBox;

    juce::TextButton playPauseButton;
    juce::TextButton dragButton;  // Drag to DAW button
    juce::TextButton addToDAWButton;  // Add to DAW at playhead
    juce::TextButton zoomInButton;
    juce::TextButton zoomOutButton;
    juce::ComboBox transposeComboBox;    // Transpose dropdown
    int transposeAmount = 0;              // Current transpose in semitones
    bool isPlaying = true;
    juce::ToggleButton syncToHostToggle{"DAW Sync"};
    juce::Slider transportSlider;
    juce::Slider velocitySlider;
    juce::Label velocityLabel;
    juce::Label timeDisplayLabel;  // Shows elapsed / total time
    MIDINoteViewer midiNoteViewer;

    std::vector<Library> libraries;
    std::vector<MIDIFileInfo> allFiles;
    std::vector<MIDIFileInfo> filteredFiles;
    std::vector<juce::String> recentlyPlayed;  // Recently played file paths
    int selectedLibraryIndex = 0;  // 0=All, 1=Favorites, 2=Recently Played, 3+=user libraries
    juce::StringArray detectedKeys;

    int selectedFileIndex = -1;
    juce::String pendingSelectedFilePath;
    bool fileLoaded = false;
    bool isQuantized = false;
    double playbackStartTime = 0;
    double playbackStartBeat = 0;
    int playbackNoteIndex = 0;

    // Host sync state
    double lastHostBpm = 120.0;
    double midiFileBpm = 120.0;
    double midiFileDuration = 0.0;       // Duration in seconds (rounded to bars)
    double midiFileDurationBeats = 0.0;  // Duration in beats
    double currentPlaybackPosition = 0.0;  // 0.0 to 1.0 for progress display
    bool wasHostPlaying = false;
    double lastBeatPosition = 0;

    // Pending file change
    bool pendingFileChange = false;
    int pendingFileIndex = -1;

    // Background analysis queue for large libraries
    std::vector<size_t> analysisQueue;
    size_t analysisIndex = 0;
    static constexpr int FILES_PER_TICK = 5;  // Analyze 5 files per timer tick
    int spinnerFrame = 0;  // Animation frame for loading spinners

    // Background file scanning
    std::vector<size_t> scanQueue;  // Library indices to scan
    std::unique_ptr<juce::DirectoryIterator> currentDirIterator;
    size_t currentScanLibraryIndex = 0;
    bool isScanningFiles = false;

    juce::MidiFile currentMidiFile;
    juce::MidiMessageSequence playbackSequence;
    std::unique_ptr<juce::FileChooser> fileChooser;

    MIDIScaleDetector::MIDIScalePlugin* pluginProcessor = nullptr;

    // License management
    LicenseManager licenseManager;
    bool showLicenseDialog = false;
    juce::TextEditor licenseKeyInput;
    juce::TextButton activateLicenseBtn{"Activate"};
    juce::TextButton deactivateLicenseBtn{"Deactivate"};
    juce::Label licenseStatusLabel;
    void showLicenseActivation();
    void hideLicenseDialog();

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
    void playNextFile();  // Play next or random file
    void applyTransposeToPlayback();  // Apply transpose to current playback
    void revealInFinder(const juce::String& path);
    void toggleFavorite(int row);
    void saveFavorites();
    void loadFavorites();
    void saveRecentlyPlayed();
    void loadRecentlyPlayed();
    void addToRecentlyPlayed(const juce::String& filePath);
    void quantizeMidi();
    void selectAndPreview(int row);

    // Utility for drag and drop with temp file copy
    juce::File makeTempCopyForDrag(const juce::File& src) {
        auto cacheDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                            .getChildFile("MIDIXplorerDrags");
        cacheDir.createDirectory();
        auto dest = cacheDir.getChildFile(src.getFileName());
        src.copyFileTo(dest);
        return dest;
    }

    // Persistence
    void saveLibraries();
    void loadLibraries();
    void saveFileCache();
    void loadFileCache();
    juce::File getSettingsFile();
    juce::File getCacheFile();

    double getHostBpm();
    double getHostBeatPosition();
    bool isHostPlaying();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MIDIXplorerEditor)
};

