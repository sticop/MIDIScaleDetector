#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Standalone/LicenseManager.h"
#include "../Version.h"

namespace MIDIScaleDetector {
    class MIDIScalePlugin;
}

//==============================================================================
// Settings Dialog Component - Scaler-style tabbed settings window
//==============================================================================
class SettingsDialogComponent : public juce::Component
{
public:
    enum class Tab { Registration, Credits, Help, Legal };

    SettingsDialogComponent(LicenseManager& lm, juce::AudioDeviceManager* adm = nullptr,
                            std::function<void(float)> volCallback = nullptr, float initialVol = 1.0f)
        : licenseManager(lm), audioDeviceManager(adm), volumeCallback(volCallback), currentVolume(initialVol)
    {
        setSize(750, 500);

        // Add sidebar buttons
        addAndMakeVisible(registrationBtn);
        addAndMakeVisible(creditsBtn);
        addAndMakeVisible(helpBtn);
        addAndMakeVisible(legalBtn);

        registrationBtn.setButtonText("Registration");
        creditsBtn.setButtonText("Credits");
        helpBtn.setButtonText("Help");
        legalBtn.setButtonText("Legal");

        auto setupBtn = [this](juce::TextButton& btn, Tab t) {
            btn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2f2f2f));
            btn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff0078d4));
            btn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            btn.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
            btn.setClickingTogglesState(true);
            btn.setRadioGroupId(1);
            btn.onClick = [this, t]() { setCurrentTab(t); };
        };

        setupBtn(registrationBtn, Tab::Registration);
        setupBtn(creditsBtn, Tab::Credits);
        setupBtn(helpBtn, Tab::Help);
        setupBtn(legalBtn, Tab::Legal);

        // Audio settings (only for standalone)
        if (audioDeviceManager != nullptr) {
            audioSettingsBtn = std::make_unique<juce::TextButton>("Audio Settings");
            audioSettingsBtn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2f2f2f));
            audioSettingsBtn->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            audioSettingsBtn->onClick = [this]() { showAudioSettings(); };
            addAndMakeVisible(*audioSettingsBtn);
        }

        // Clear Cache button
        clearCacheBtn = std::make_unique<juce::TextButton>("Clear Cache");
        clearCacheBtn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff8b4444));
        clearCacheBtn->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        clearCacheBtn->onClick = [this]() { clearCache(); };
        addAndMakeVisible(*clearCacheBtn);

        // Help buttons
        addAndMakeVisible(websiteBtn);
        addAndMakeVisible(tutorialsBtn);
        addAndMakeVisible(manualBtn);
        addAndMakeVisible(forumBtn);
        addAndMakeVisible(faqBtn);

        websiteBtn.setButtonText("Our Website");
        tutorialsBtn.setButtonText("Video Tutorials");
        manualBtn.setButtonText("Open Manual");
        forumBtn.setButtonText("Community Forum");
        faqBtn.setButtonText("FAQ");

        auto styleHelpBtn = [](juce::TextButton& btn) {
            btn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0078d4));
            btn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        };

        styleHelpBtn(websiteBtn);
        styleHelpBtn(tutorialsBtn);
        styleHelpBtn(manualBtn);
        styleHelpBtn(forumBtn);
        styleHelpBtn(faqBtn);

        websiteBtn.onClick = []() { juce::URL("https://midixplorer.com").launchInDefaultBrowser(); };
        tutorialsBtn.onClick = []() { juce::URL("https://midixplorer.com/tutorials").launchInDefaultBrowser(); };
        manualBtn.onClick = []() { juce::URL("https://midixplorer.com/manual").launchInDefaultBrowser(); };
        forumBtn.onClick = []() { juce::URL("https://midixplorer.com/forum").launchInDefaultBrowser(); };
        faqBtn.onClick = []() { juce::URL("https://midixplorer.com/faq").launchInDefaultBrowser(); };

        setCurrentTab(Tab::Registration);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff2a2a2a));

        // Sidebar
        g.setColour(juce::Colour(0xff252525));
        g.fillRect(0, 0, sidebarWidth, getHeight());

        // Content area
        auto contentArea = getLocalBounds().withTrimmedLeft(sidebarWidth).reduced(20);

        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(24.0f).withStyle("Bold")));

        switch (currentTab) {
            case Tab::Registration:
                paintRegistration(g, contentArea);
                break;
            case Tab::Credits:
                paintCredits(g, contentArea);
                break;
            case Tab::Help:
                // Help buttons are positioned in resized()
                break;
            case Tab::Legal:
                paintLegal(g, contentArea);
                break;
        }
    }

    void paintRegistration(juce::Graphics& g, juce::Rectangle<int> area)
    {
        g.drawText("Welcome to MIDI Xplorer", area.removeFromTop(40), juce::Justification::centredLeft);
        area.removeFromTop(20);

        g.setFont(juce::Font(juce::FontOptions(14.0f)));
        g.setColour(juce::Colours::lightgrey);

        auto& license = licenseManager;
        if (license.isLicenseValid()) {
            auto info = license.getLicenseInfo();
            g.drawText("You have successfully registered your copy of MIDI Xplorer.", area.removeFromTop(25), juce::Justification::centredLeft);
            g.drawText("Licensed to: " + info.customerName, area.removeFromTop(25), juce::Justification::centredLeft);
            g.drawText("License type: " + info.licenseType, area.removeFromTop(25), juce::Justification::centredLeft);
        } else if (license.isInTrialPeriod()) {
            int days = license.getTrialDaysRemaining();
            g.drawText("Trial Mode - " + juce::String(days) + " days remaining", area.removeFromTop(25), juce::Justification::centredLeft);
            area.removeFromTop(10);
            g.drawText("Enter your license key to activate:", area.removeFromTop(25), juce::Justification::centredLeft);
        } else {
            g.setColour(juce::Colours::orange);
            g.drawText("Trial Expired - Please activate a license to continue.", area.removeFromTop(25), juce::Justification::centredLeft);
        }
    }

    void paintCredits(juce::Graphics& g, juce::Rectangle<int> area)
    {
        // Logo area
        auto logoArea = area.removeFromTop(80);
        g.setFont(juce::Font(juce::FontOptions(36.0f).withStyle("Bold")));
        g.drawText("MIDI Xplorer", logoArea, juce::Justification::centred);

        area.removeFromTop(20);
        g.setFont(juce::Font(juce::FontOptions(13.0f)));
        g.setColour(juce::Colours::grey);

        auto drawCredit = [&](const juce::String& label, const juce::String& value) {
            auto row = area.removeFromTop(22);
            g.setColour(juce::Colours::grey);
            g.drawText(label + ":", row.removeFromLeft(180), juce::Justification::centredRight);
            row.removeFromLeft(10);
            g.setColour(juce::Colours::white);
            g.drawText(value, row, juce::Justification::centredLeft);
        };

        drawCredit("Developer", "MIDI Xplorer Team");
        drawCredit("Version", MIDIXPLORER_VERSION_STRING);
        drawCredit("Build Date", juce::String(__DATE__));

        area.removeFromTop(30);
        g.setColour(juce::Colours::grey);
        g.setFont(juce::Font(juce::FontOptions(12.0f)));
        g.drawText(juce::CharPointer_UTF8("\xc2\xa9 2024 MIDI Xplorer. All rights reserved."),
                   area.removeFromTop(20), juce::Justification::centred);
    }

    void paintLegal(juce::Graphics& g, juce::Rectangle<int> area)
    {
        g.setFont(juce::Font(juce::FontOptions(13.0f)));
        g.setColour(juce::Colours::lightgrey);
        g.drawFittedText(
            "VST is a trademark of Steinberg Media Technologies GmbH, "
            "registered in Europe and other countries.\n\n"
            "Audio Units is a trademark of Apple Inc.\n\n"
            "MIDI is a trademark of the MIDI Manufacturers Association.\n\n"
            "All other trademarks are the property of their respective owners.",
            area, juce::Justification::topLeft, 10);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        auto sidebar = bounds.removeFromLeft(sidebarWidth);
        sidebar.removeFromTop(10);

        // Sidebar buttons
        registrationBtn.setBounds(sidebar.removeFromTop(40).reduced(5, 2));
        creditsBtn.setBounds(sidebar.removeFromTop(40).reduced(5, 2));
        helpBtn.setBounds(sidebar.removeFromTop(40).reduced(5, 2));
        legalBtn.setBounds(sidebar.removeFromTop(40).reduced(5, 2));

        if (audioSettingsBtn) {
            sidebar.removeFromTop(20);
            audioSettingsBtn->setBounds(sidebar.removeFromTop(40).reduced(5, 2));
        }

        if (clearCacheBtn) {
            sidebar.removeFromTop(10);
            clearCacheBtn->setBounds(sidebar.removeFromTop(40).reduced(5, 2));
        }

        // Help buttons - only visible in Help tab
        bool helpVisible = (currentTab == Tab::Help);
        websiteBtn.setVisible(helpVisible);
        tutorialsBtn.setVisible(helpVisible);
        manualBtn.setVisible(helpVisible);
        forumBtn.setVisible(helpVisible);
        faqBtn.setVisible(helpVisible);

        if (helpVisible) {
            auto content = bounds.reduced(40);
            content.removeFromTop(60);

            int btnWidth = 200;
            int btnHeight = 40;
            int spacing = 15;

            auto centerX = content.getCentreX() - btnWidth / 2;

            websiteBtn.setBounds(centerX, content.getY(), btnWidth, btnHeight);
            tutorialsBtn.setBounds(centerX, content.getY() + (btnHeight + spacing), btnWidth, btnHeight);
            manualBtn.setBounds(centerX, content.getY() + 2 * (btnHeight + spacing), btnWidth, btnHeight);
            forumBtn.setBounds(centerX, content.getY() + 3 * (btnHeight + spacing), btnWidth, btnHeight);
            faqBtn.setBounds(centerX, content.getY() + 4 * (btnHeight + spacing), btnWidth, btnHeight);
        }
    }

    void setCurrentTab(Tab tab)
    {
        currentTab = tab;
        registrationBtn.setToggleState(tab == Tab::Registration, juce::dontSendNotification);
        creditsBtn.setToggleState(tab == Tab::Credits, juce::dontSendNotification);
        helpBtn.setToggleState(tab == Tab::Help, juce::dontSendNotification);
        legalBtn.setToggleState(tab == Tab::Legal, juce::dontSendNotification);
        resized();
        repaint();
    }

    void showAudioSettings()
    {
        if (audioDeviceManager == nullptr) return;

        // Create a component that combines volume control with device selector
        auto* content = new juce::Component();
        content->setSize(500, 520);

        // Volume label
        auto* volLabel = new juce::Label();
        volLabel->setText("Piano Instrument Volume:", juce::dontSendNotification);
        volLabel->setFont(juce::Font(juce::FontOptions(14.0f).withStyle("Bold")));
        volLabel->setColour(juce::Label::textColourId, juce::Colours::white);
        volLabel->setBounds(10, 10, 200, 20);
        content->addAndMakeVisible(volLabel);

        // Volume slider
        auto* volSlider = new juce::Slider();
        volSlider->setSliderStyle(juce::Slider::LinearHorizontal);
        volSlider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 25);
        volSlider->setRange(0.0, 1.0, 0.01);
        volSlider->setValue(currentVolume);
        volSlider->setColour(juce::Slider::backgroundColourId, juce::Colour(0xff3a3a3a));
        volSlider->setColour(juce::Slider::trackColourId, juce::Colour(0xff0078d4));
        volSlider->setColour(juce::Slider::thumbColourId, juce::Colours::white);
        volSlider->setBounds(10, 30, 480, 25);
        if (volumeCallback) {
            volSlider->onValueChange = [this, volSlider]() {
                currentVolume = (float)volSlider->getValue();
                if (volumeCallback) volumeCallback(currentVolume);
            };
        }
        content->addAndMakeVisible(volSlider);

        // Device selector
        auto* selector = new juce::AudioDeviceSelectorComponent(
            *audioDeviceManager, 0, 0, 0, 2, false, false, true, false);
        selector->setBounds(0, 65, 500, 455);
        content->addAndMakeVisible(selector);

        juce::DialogWindow::LaunchOptions options;
        options.content.setOwned(content);
        options.dialogTitle = "Audio/MIDI Settings";
        options.dialogBackgroundColour = juce::Colour(0xff2a2a2a);
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = false;
        options.launchAsync();
    }

private:
    LicenseManager& licenseManager;
    juce::AudioDeviceManager* audioDeviceManager = nullptr;
    std::function<void(float)> volumeCallback;
    float currentVolume = 1.0f;

    Tab currentTab = Tab::Registration;
    static constexpr int sidebarWidth = 200;

    juce::TextButton registrationBtn, creditsBtn, helpBtn, legalBtn;
    std::unique_ptr<juce::TextButton> audioSettingsBtn;
    std::unique_ptr<juce::TextButton> clearCacheBtn;

    // Help buttons
    juce::TextButton websiteBtn, tutorialsBtn, manualBtn, forumBtn, faqBtn;

    void clearCache()
    {
        // Clear the cache directory
        auto cacheDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                            .getChildFile("MIDI Xplorer").getChildFile("cache");

        if (cacheDir.exists())
        {
            cacheDir.deleteRecursively();
        }

        // Also clear the analysis cache file
        auto analysisCache = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                                 .getChildFile("MIDI Xplorer").getChildFile("analysis_cache.json");
        if (analysisCache.exists())
        {
            analysisCache.deleteFile();
        }

        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon,
            "Cache Cleared",
            "Cache has been cleared successfully.\nRestart the app to rescan your libraries.",
            "OK");
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsDialogComponent)
};

//==============================================================================
// Drag to DAW Button - allows dragging MIDI files to external applications
//==============================================================================
class DragToDAWButton : public juce::Component, public juce::SettableTooltipClient {
public:
    DragToDAWButton() {
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    }

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();

        // Background
        g.setColour(isHovering ? juce::Colour(0xff4a6a4a) : juce::Colour(0xff3a5a3a));
        g.fillRoundedRectangle(bounds, 4.0f);

        // Border
        g.setColour(juce::Colour(0xff5a8a5a));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);

        // Text
        g.setColour(juce::Colours::lightgreen);
        g.setFont(12.0f);
        g.drawText("Drag to DAW", bounds, juce::Justification::centred);
    }

    void mouseEnter(const juce::MouseEvent&) override {
        isHovering = true;
        repaint();
    }

    void mouseExit(const juce::MouseEvent&) override {
        isHovering = false;
        repaint();
    }

    void mouseDrag(const juce::MouseEvent& e) override {
        if (!isDragging && e.getDistanceFromDragStart() > 4 && currentFilePath.isNotEmpty()) {
            isDragging = true;
            juce::File midiFile(currentFilePath);
            if (midiFile.existsAsFile()) {
                // Perform external drag to OS/DAW
                juce::DragAndDropContainer* container = juce::DragAndDropContainer::findParentDragContainerFor(this);
                if (container != nullptr) {
                    juce::StringArray files;
                    files.add(currentFilePath);
                    container->performExternalDragDropOfFiles(files, false);
                }
            }
        }
    }

    void mouseUp(const juce::MouseEvent&) override {
        isDragging = false;
    }

    void setCurrentFile(const juce::String& filePath) {
        currentFilePath = filePath;
    }

private:
    bool isHovering = false;
    bool isDragging = false;
    juce::String currentFilePath;
};

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

    // License status helpers
    bool isLicenseExpiredOrTrialExpired() const;
    int getLicenseStatusBarHeight() const;

    // Audio device manager setter (called by standalone app)
    void setAudioDeviceManager(juce::AudioDeviceManager* adm) { audioDeviceManager = adm; }

    // Volume control callback (called by standalone app)
    void setVolumeCallback(std::function<void(float)> callback, float initialVol) {
        volumeCallback = callback;
        currentVolume = initialVol;
    }

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
        juce::String mood = "---";  // Detected mood (Happy, Melancholic, Energetic, etc.)
        juce::StringArray tags;  // Tags extracted from filename (instrument type, genre, etc.)
        bool analyzed = false;
        bool isAnalyzing = false;  // Currently being analyzed
        bool favorite = false;  // Marked as favorite
        bool containsChords = false;  // Has simultaneous notes (3+ at once)
        bool containsSingleNotes = false;  // Has melodic single notes
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
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
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
        void setVelocityScale(float scale) { velocityScale = juce::jlimit(0.0f, 2.0f, scale); repaint(); }
        static constexpr int PIANO_WIDTH = 40;  // Width of piano keyboard on left

        // Fullscreen mode
        bool isFullscreen() const { return fullscreenMode; }
        void setFullscreen(bool fs) { fullscreenMode = fs; }
        std::function<void()> onFullscreenToggle;  // Callback when fullscreen button clicked
        std::function<void(double)> onPlayheadJump;  // Callback when user clicks to jump playhead (position 0-1)

    public:
        MIDINoteViewer() { setMouseCursor(juce::MouseCursor::CrosshairCursor); }
        void paint(juce::Graphics& g) override;
        void resized() override;
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
        float velocityScale = 1.0f;
        std::vector<int> playingNotes;  // Currently playing notes
        bool fullscreenMode = false;

        // Fullscreen toggle button bounds
        juce::Rectangle<int> fullscreenBtnBounds;

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
    juce::ComboBox contentTypeFilterCombo;  // Filter by chords/notes
    juce::ComboBox tagFilterCombo;  // Filter by filename-extracted tags
    juce::ComboBox sortCombo;
    juce::ComboBox quantizeCombo;
    juce::TextEditor searchBox;
    std::unique_ptr<DraggableListBox> fileListBox;

    juce::TextButton playPauseButton;
    juce::TextButton addToDAWButton;  // Add to DAW at playhead
    DragToDAWButton dragToDAWButton;  // Drag to external DAW/Finder
    juce::TextButton zoomInButton;
    juce::TextButton zoomOutButton;
    juce::TextButton settingsButton;  // Gear icon for settings menu
    juce::ComboBox transposeComboBox;    // Transpose dropdown
    juce::TextButton transposeUpButton;  // +1 semitone
    juce::TextButton transposeDownButton;  // -1 semitone
    int transposeAmount = 0;              // Current transpose in semitones
    bool isPlaying = true;
    juce::ToggleButton syncToHostToggle{"DAW Sync"};
    juce::Slider transportSlider;
    juce::Slider velocitySlider;
    juce::Label velocityLabel;
    bool velocityFaderTouched = false;  // Track if velocity fader was manually adjusted
    juce::Label timeDisplayLabel;  // Shows elapsed / total time
    MIDINoteViewer midiNoteViewer;
    bool midiViewerFullscreen = false;  // Fullscreen mode for MIDI viewer

    std::vector<Library> libraries;
    std::vector<MIDIFileInfo> allFiles;
    std::vector<MIDIFileInfo> filteredFiles;
    std::vector<juce::String> recentlyPlayed;  // Recently played file paths
    int selectedLibraryIndex = 0;  // 0=All, 1=Favorites, 2=Recently Played, 3+=user libraries
    juce::StringArray detectedKeys;
    juce::StringArray allTags;  // All unique tags extracted from filenames

    int selectedFileIndex = -1;
    juce::String pendingSelectedFilePath;
    bool suppressSelectionChange = false;
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
    int analysisSaveCounter = 0;  // Counter for periodic cache saving
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

    // Audio device manager (set by standalone app, nullptr in plugin mode)
    juce::AudioDeviceManager* audioDeviceManager = nullptr;

    // Volume callback (set by standalone app)
    std::function<void(float)> volumeCallback;
    float currentVolume = 1.0f;

    // License management - use singleton reference
    LicenseManager& licenseManager;
    juce::TextButton statusBarActivateBtn{"Activate License"};  // Button in status bar
    void showLicenseActivation();
    void showSettingsMenu();
    void showHelpDialog(const juce::String& topic);
    void showAboutDialog();

    void addLibrary();
    void scanLibraries();
    void scanLibrary(size_t index);
    void refreshLibrary(size_t index);
    void analyzeFile(size_t index);
    void filterFiles();
    void sortFiles();
    void updateKeyFilterFromDetectedScales();
    void updateContentTypeFilter();  // Update chord/note filter with counts
    void updateTagFilter();  // Update tag filter with available tags
    juce::StringArray extractTagsFromFilename(const juce::String& filename);  // Extract tags from filename
    juce::String extractKeyFromFilename(const juce::String& filename);  // Extract key/scale from filename
    void loadSelectedFile();
    void scheduleFileChangeTo(int row);
    void stopPlayback();
    void restartPlayback();
    void playNextFile();  // Play next or random file
    void updateTransposeComboSelection();
    void applyTransposeToPlayback();  // Apply transpose to current playback
    void restoreSelectionFromCurrentFile();
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
