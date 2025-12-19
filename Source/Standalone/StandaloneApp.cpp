#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "PianoSynthesizer.h"

/**
 * Standalone MIDI Xplorer Application
 * Full-featured MIDI file browser and player with built-in piano synthesizer
 */
class MIDIXplorerStandaloneApp : public juce::JUCEApplication
{
public:
    MIDIXplorerStandaloneApp() = default;
    
    const juce::String getApplicationName() override { return "MIDI Xplorer"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }
    
    void initialise(const juce::String&) override
    {
        mainWindow = std::make_unique<MainWindow>(getApplicationName());
    }
    
    void shutdown() override
    {
        mainWindow = nullptr;
    }
    
    void systemRequestedQuit() override
    {
        quit();
    }
    
    void anotherInstanceStarted(const juce::String&) override {}
    
private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow(const juce::String& name)
            : DocumentWindow(name, juce::Colour(0xff1a1a1a), DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(), true);
            
            #if JUCE_IOS || JUCE_ANDROID
            setFullScreen(true);
            #else
            setResizable(true, true);
            centreWithSize(getWidth(), getHeight());
            #endif
            
            setVisible(true);
        }
        
        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }
        
    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };
    
    /**
     * Main Component - Audio engine + MIDI browser UI
     */
    class MainComponent : public juce::Component,
                          public juce::Timer,
                          public juce::DragAndDropContainer,
                          public juce::FileDragAndDropTarget
    {
    public:
        MainComponent()
        {
            setSize(1200, 750);
            
            // Initialize audio
            auto audioError = audioDeviceManager.initialiseWithDefaultDevices(0, 2);
            if (audioError.isNotEmpty()) {
                juce::Logger::writeToLog("Audio init error: " + audioError);
            }
            
            audioDeviceManager.addAudioCallback(&audioSourcePlayer);
            audioSourcePlayer.setSource(&audioProcessor);
            
            pianoSynth.prepareToPlay(44100, 512);
            
            // Setup UI
            setupUI();
            
            // Load settings and libraries
            loadSettings();
            
            startTimer(20);
        }
        
        ~MainComponent() override
        {
            stopTimer();
            saveSettings();
            audioSourcePlayer.setSource(nullptr);
            audioDeviceManager.removeAudioCallback(&audioSourcePlayer);
        }
        
        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colour(0xff1a1a1a));
            
            // Sidebar
            g.setColour(juce::Colour(0xff252525));
            g.fillRect(0, 0, 200, getHeight());
            
            g.setColour(juce::Colour(0xff3a3a3a));
            g.drawVerticalLine(200, 0.0f, (float)getHeight());
            
            // Title bar
            g.setColour(juce::Colour(0xff00d4ff));
            g.setFont(juce::Font(juce::FontOptions(18.0f).withStyle("Bold")));
            g.drawText(juce::String::fromUTF8("🎹 MIDI Xplorer"), 10, 10, 180, 30, juce::Justification::centred);
        }
        
        void resized() override
        {
            auto area = getLocalBounds();
            
            // Sidebar
            auto sidebar = area.removeFromLeft(200);
            sidebar.removeFromTop(50); // Title area
            
            auto sidebarTop = sidebar.removeFromTop(30);
            addLibraryBtn.setBounds(sidebarTop.reduced(8, 4));
            libraryListBox.setBounds(sidebar.reduced(4));
            
            // Main area
            area.removeFromLeft(1);
            
            // Search
            auto searchRow = area.removeFromTop(40);
            searchBox.setBounds(searchRow.reduced(8, 6));
            
            // Controls
            auto topBar = area.removeFromTop(32);
            topBar = topBar.reduced(8, 2);
            fileCountLabel.setBounds(topBar.removeFromLeft(80));
            keyFilterCombo.setBounds(topBar.removeFromLeft(110).reduced(2));
            sortCombo.setBounds(topBar.removeFromLeft(110).reduced(2));
            volumeSlider.setBounds(topBar.removeFromRight(100).reduced(2));
            volumeLabel.setBounds(topBar.removeFromRight(55));
            
            // Transport
            auto transport = area.removeFromBottom(50).reduced(8, 8);
            playBtn.setBounds(transport.removeFromLeft(50));
            transport.removeFromLeft(8);
            stopBtn.setBounds(transport.removeFromLeft(50));
            transport.removeFromLeft(8);
            loopBtn.setBounds(transport.removeFromLeft(50));
            transport.removeFromLeft(20);
            
            audioSettingsBtn.setBounds(transport.removeFromRight(120));
            transport.removeFromRight(8);
            transportSlider.setBounds(transport);
            
            // MIDI viewer
            auto viewerArea = area.removeFromBottom(200);
            midiViewer.setBounds(viewerArea.reduced(8));
            
            // File list
            fileListBox.setBounds(area.reduced(8));
        }
        
        void timerCallback() override
        {
            updatePlayback();
        }
        
        bool isInterestedInFileDrag(const juce::StringArray& files) override
        {
            for (const auto& f : files) {
                if (f.endsWithIgnoreCase(".mid") || f.endsWithIgnoreCase(".midi")) {
                    return true;
                }
            }
            return false;
        }
        
        void filesDropped(const juce::StringArray& files, int, int) override
        {
            for (const auto& f : files) {
                if (f.endsWithIgnoreCase(".mid") || f.endsWithIgnoreCase(".midi")) {
                    loadMidiFile(juce::File(f));
                    break;
                }
            }
        }
        
    private:
        // Audio
        juce::AudioDeviceManager audioDeviceManager;
        juce::AudioSourcePlayer audioSourcePlayer;
        
        class AudioProcessor : public juce::AudioSource
        {
        public:
            AudioProcessor(MainComponent& o) : owner(o) {}
            
            void prepareToPlay(int, double sampleRate) override {
                owner.pianoSynth.prepareToPlay(sampleRate, 512);
            }
            
            void releaseResources() override {}
            
            void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override {
                bufferToFill.clearActiveBufferRegion();
                
                juce::MidiBuffer midiBuffer;
                
                // Process any pending MIDI from playback
                {
                    const juce::ScopedLock sl(owner.midiLock);
                    midiBuffer.swapWith(owner.pendingMidi);
                }
                
                owner.pianoSynth.processBlock(*bufferToFill.buffer, midiBuffer);
                
                // Apply volume
                bufferToFill.buffer->applyGain(owner.volume);
            }
            
        private:
            MainComponent& owner;
        };
        
        AudioProcessor audioProcessor{*this};
        PianoSynthesizer pianoSynth;
        juce::CriticalSection midiLock;
        juce::MidiBuffer pendingMidi;
        float volume = 0.8f;
        
        // MIDI playback
        juce::MidiFile currentMidi;
        juce::MidiMessageSequence playbackSequence;
        bool isPlaying = false;
        bool isLooping = false;
        double playbackPosition = 0.0;
        double midiDuration = 0.0;
        int playbackNoteIndex = 0;
        juce::int64 lastPlaybackTime = 0;
        
        // UI Components
        juce::TextButton addLibraryBtn{"+ Add Library"};
        juce::ListBox libraryListBox{"Libraries"};
        juce::ListBox fileListBox{"Files"};
        juce::TextEditor searchBox;
        juce::Label fileCountLabel;
        juce::ComboBox keyFilterCombo;
        juce::ComboBox sortCombo;
        juce::Slider volumeSlider;
        juce::Label volumeLabel;
        juce::TextButton playBtn{juce::String::fromUTF8("\u25B6")};
        juce::TextButton stopBtn{juce::String::fromUTF8("\u23F9")};
        juce::TextButton loopBtn{juce::String::fromUTF8("\u21BA")};
        juce::Slider transportSlider;
        juce::TextButton audioSettingsBtn{"Audio Settings"};
        
        // MIDI Viewer Component
        class MIDIViewer : public juce::Component
        {
        public:
            void paint(juce::Graphics& g) override
            {
                g.fillAll(juce::Colour(0xff121212));
                g.setColour(juce::Colour(0xff333333));
                g.drawRect(getLocalBounds());
                
                if (sequence == nullptr || sequence->getNumEvents() == 0) {
                    g.setColour(juce::Colours::grey);
                    g.setFont(14.0f);
                    g.drawText("No MIDI file loaded - drop a .mid file here", getLocalBounds(), juce::Justification::centred);
                    return;
                }
                
                // Draw piano roll
                // ... (simplified for now)
                g.setColour(juce::Colours::grey);
                g.setFont(12.0f);
                g.drawText("MIDI Preview - " + juce::String(sequence->getNumEvents()) + " events", 
                           getLocalBounds(), juce::Justification::centred);
            }
            
            void setSequence(const juce::MidiMessageSequence* seq) {
                sequence = seq;
                repaint();
            }
            
        private:
            const juce::MidiMessageSequence* sequence = nullptr;
        };
        
        MIDIViewer midiViewer;
        
        // File data
        struct MIDIFileInfo {
            juce::String name;
            juce::String path;
            juce::String key;
            double duration = 0.0;
        };
        
        std::vector<juce::String> libraries;
        std::vector<MIDIFileInfo> allFiles;
        std::vector<MIDIFileInfo> filteredFiles;
        
        // List models
        class LibraryListModel : public juce::ListBoxModel {
        public:
            LibraryListModel(MainComponent& o) : owner(o) {}
            int getNumRows() override { return 1 + (int)owner.libraries.size(); }
            void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) override {
                if (selected) g.fillAll(juce::Colour(0xff0f3460));
                g.setColour(juce::Colours::white);
                g.setFont(13.0f);
                if (row == 0) {
                    g.drawText("All Files (" + juce::String(owner.allFiles.size()) + ")", 8, 0, w - 16, h, juce::Justification::centredLeft);
                } else if (row - 1 < (int)owner.libraries.size()) {
                    g.drawText(juce::File(owner.libraries[(size_t)(row - 1)]).getFileName(), 8, 0, w - 16, h, juce::Justification::centredLeft);
                }
            }
            void listBoxItemClicked(int, const juce::MouseEvent&) override { owner.filterFiles(); }
        private:
            MainComponent& owner;
        };
        
        class FileListModel : public juce::ListBoxModel {
        public:
            FileListModel(MainComponent& o) : owner(o) {}
            int getNumRows() override { return (int)owner.filteredFiles.size(); }
            void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) override {
                if (row >= (int)owner.filteredFiles.size()) return;
                if (selected) g.fillAll(juce::Colour(0xff0f3460));
                g.setColour(juce::Colours::white);
                g.setFont(13.0f);
                g.drawText(owner.filteredFiles[(size_t)row].name, 8, 0, w - 16, h, juce::Justification::centredLeft);
            }
            void selectedRowsChanged(int lastRowSelected) override {
                if (lastRowSelected >= 0 && lastRowSelected < (int)owner.filteredFiles.size()) {
                    owner.loadMidiFile(juce::File(owner.filteredFiles[(size_t)lastRowSelected].path));
                }
            }
        private:
            MainComponent& owner;
        };
        
        std::unique_ptr<LibraryListModel> libraryModel;
        std::unique_ptr<FileListModel> fileModel;
        
        void setupUI()
        {
            // Add library button
            addLibraryBtn.onClick = [this]() { addLibrary(); };
            addAndMakeVisible(addLibraryBtn);
            
            // Library list
            libraryModel = std::make_unique<LibraryListModel>(*this);
            libraryListBox.setModel(libraryModel.get());
            libraryListBox.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff2a2a2a));
            libraryListBox.setRowHeight(28);
            addAndMakeVisible(libraryListBox);
            
            // File list
            fileModel = std::make_unique<FileListModel>(*this);
            fileListBox.setModel(fileModel.get());
            fileListBox.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff1e1e1e));
            fileListBox.setRowHeight(24);
            addAndMakeVisible(fileListBox);
            
            // Search
            searchBox.setTextToShowWhenEmpty("Search MIDI files...", juce::Colours::grey);
            searchBox.onTextChange = [this]() { filterFiles(); };
            addAndMakeVisible(searchBox);
            
            // Filters
            fileCountLabel.setText("0 files", juce::dontSendNotification);
            fileCountLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
            addAndMakeVisible(fileCountLabel);
            
            keyFilterCombo.addItem("All Keys", 1);
            keyFilterCombo.setSelectedId(1);
            keyFilterCombo.onChange = [this]() { filterFiles(); };
            addAndMakeVisible(keyFilterCombo);
            
            sortCombo.addItem("Sort: Name", 1);
            sortCombo.addItem("Sort: Duration", 2);
            sortCombo.setSelectedId(1);
            sortCombo.onChange = [this]() { filterFiles(); };
            addAndMakeVisible(sortCombo);
            
            // Volume
            volumeLabel.setText("Volume:", juce::dontSendNotification);
            volumeLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
            addAndMakeVisible(volumeLabel);
            
            volumeSlider.setRange(0.0, 1.0, 0.01);
            volumeSlider.setValue(0.8);
            volumeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
            volumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
            volumeSlider.onValueChange = [this]() { volume = (float)volumeSlider.getValue(); };
            addAndMakeVisible(volumeSlider);
            
            // Transport
            playBtn.onClick = [this]() { togglePlayback(); };
            addAndMakeVisible(playBtn);
            
            stopBtn.onClick = [this]() { stopPlayback(); };
            addAndMakeVisible(stopBtn);
            
            loopBtn.onClick = [this]() { isLooping = !isLooping; loopBtn.setToggleState(isLooping, juce::dontSendNotification); };
            loopBtn.setClickingTogglesState(true);
            addAndMakeVisible(loopBtn);
            
            transportSlider.setRange(0.0, 1.0, 0.001);
            transportSlider.setSliderStyle(juce::Slider::LinearHorizontal);
            transportSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
            transportSlider.onDragEnd = [this]() { seekToPosition(transportSlider.getValue()); };
            addAndMakeVisible(transportSlider);
            
            audioSettingsBtn.onClick = [this]() { showAudioSettings(); };
            addAndMakeVisible(audioSettingsBtn);
            
            // MIDI Viewer
            addAndMakeVisible(midiViewer);
        }
        
        void addLibrary()
        {
            auto chooser = std::make_shared<juce::FileChooser>("Select MIDI Library Folder", juce::File(), "");
            chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                [this, chooser](const juce::FileChooser& fc) {
                    auto result = fc.getResult();
                    if (result.isDirectory()) {
                        libraries.push_back(result.getFullPathName());
                        scanLibrary(result);
                        libraryListBox.updateContent();
                        filterFiles();
                    }
                });
        }
        
        void scanLibrary(const juce::File& dir)
        {
            for (const auto& entry : juce::RangedDirectoryIterator(dir, true, "*.mid;*.midi")) {
                MIDIFileInfo info;
                info.name = entry.getFile().getFileName();
                info.path = entry.getFile().getFullPathName();
                info.key = "---";
                info.duration = 0.0;
                allFiles.push_back(info);
            }
            filterFiles();
        }
        
        void filterFiles()
        {
            juce::String search = searchBox.getText().toLowerCase();
            filteredFiles.clear();
            
            for (const auto& f : allFiles) {
                if (search.isEmpty() || f.name.toLowerCase().contains(search)) {
                    filteredFiles.push_back(f);
                }
            }
            
            fileCountLabel.setText(juce::String(filteredFiles.size()) + " files", juce::dontSendNotification);
            fileListBox.updateContent();
        }
        
        void loadMidiFile(const juce::File& file)
        {
            juce::FileInputStream stream(file);
            if (!stream.openedOk()) return;
            
            currentMidi.readFrom(stream);
            currentMidi.convertTimestampTicksToSeconds();
            
            // Merge all tracks
            playbackSequence.clear();
            midiDuration = 0.0;
            
            for (int t = 0; t < currentMidi.getNumTracks(); t++) {
                auto* track = currentMidi.getTrack(t);
                if (track) {
                    for (int i = 0; i < track->getNumEvents(); i++) {
                        auto& msg = track->getEventPointer(i)->message;
                        playbackSequence.addEvent(msg);
                        if (msg.getTimeStamp() > midiDuration) {
                            midiDuration = msg.getTimeStamp();
                        }
                    }
                }
            }
            
            playbackSequence.updateMatchedPairs();
            playbackSequence.sort();
            
            midiViewer.setSequence(&playbackSequence);
            
            // Reset playback
            playbackPosition = 0.0;
            playbackNoteIndex = 0;
            
            // Auto-play on selection
            if (!isPlaying) {
                startPlayback();
            }
        }
        
        void togglePlayback()
        {
            if (isPlaying) {
                pausePlayback();
            } else {
                startPlayback();
            }
        }
        
        void startPlayback()
        {
            isPlaying = true;
            lastPlaybackTime = juce::Time::getMillisecondCounterHiRes();
            playBtn.setButtonText(juce::String::fromUTF8("\u23F8"));
        }
        
        void pausePlayback()
        {
            isPlaying = false;
            playBtn.setButtonText(juce::String::fromUTF8("\u25B6"));
            pianoSynth.allNotesOff();
        }
        
        void stopPlayback()
        {
            isPlaying = false;
            playbackPosition = 0.0;
            playbackNoteIndex = 0;
            playBtn.setButtonText(juce::String::fromUTF8("\u25B6"));
            transportSlider.setValue(0.0, juce::dontSendNotification);
            pianoSynth.allNotesOff();
        }
        
        void seekToPosition(double pos)
        {
            playbackPosition = pos * midiDuration;
            playbackNoteIndex = 0;
            
            // Find correct note index
            for (int i = 0; i < playbackSequence.getNumEvents(); i++) {
                if (playbackSequence.getEventPointer(i)->message.getTimeStamp() >= playbackPosition) {
                    playbackNoteIndex = i;
                    break;
                }
            }
            
            pianoSynth.allNotesOff();
        }
        
        void updatePlayback()
        {
            if (!isPlaying || midiDuration <= 0) return;
            
            auto currentTime = juce::Time::getMillisecondCounterHiRes();
            double deltaTime = (currentTime - lastPlaybackTime) / 1000.0;
            lastPlaybackTime = currentTime;
            
            playbackPosition += deltaTime;
            
            // Update transport slider
            transportSlider.setValue(playbackPosition / midiDuration, juce::dontSendNotification);
            
            // Check for end of file
            if (playbackPosition >= midiDuration) {
                if (isLooping) {
                    playbackPosition = 0.0;
                    playbackNoteIndex = 0;
                } else {
                    stopPlayback();
                    return;
                }
            }
            
            // Process MIDI events
            juce::MidiBuffer buffer;
            
            while (playbackNoteIndex < playbackSequence.getNumEvents()) {
                auto* event = playbackSequence.getEventPointer(playbackNoteIndex);
                if (event->message.getTimeStamp() > playbackPosition) break;
                
                buffer.addEvent(event->message, 0);
                playbackNoteIndex++;
            }
            
            // Queue MIDI for audio thread
            {
                const juce::ScopedLock sl(midiLock);
                for (const auto metadata : buffer) {
                    pendingMidi.addEvent(metadata.getMessage(), metadata.samplePosition);
                }
            }
        }
        
        void showAudioSettings()
        {
            auto* selector = new juce::AudioDeviceSelectorComponent(
                audioDeviceManager, 0, 0, 2, 2, false, false, true, false);
            selector->setSize(500, 400);
            
            juce::DialogWindow::LaunchOptions options;
            options.content.setOwned(selector);
            options.dialogTitle = "Audio Settings";
            options.dialogBackgroundColour = juce::Colour(0xff1a1a2e);
            options.escapeKeyTriggersCloseButton = true;
            options.useNativeTitleBar = true;
            options.resizable = false;
            options.launchAsync();
        }
        
        void loadSettings()
        {
            auto settingsFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                .getChildFile("MIDI Xplorer")
                .getChildFile("standalone_settings.json");
            
            if (settingsFile.existsAsFile()) {
                auto json = juce::JSON::parse(settingsFile);
                if (json.isObject()) {
                    auto* libsArray = json.getProperty("libraries", {}).getArray();
                    if (libsArray) {
                        for (const auto& lib : *libsArray) {
                            juce::String path = lib.toString();
                            if (juce::File(path).isDirectory()) {
                                libraries.push_back(path);
                                scanLibrary(juce::File(path));
                            }
                        }
                    }
                }
            }
        }
        
        void saveSettings()
        {
            auto settingsDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                .getChildFile("MIDI Xplorer");
            settingsDir.createDirectory();
            
            auto settingsFile = settingsDir.getChildFile("standalone_settings.json");
            
            juce::DynamicObject::Ptr obj = new juce::DynamicObject();
            juce::Array<juce::var> libsArray;
            for (const auto& lib : libraries) {
                libsArray.add(lib);
            }
            obj->setProperty("libraries", libsArray);
            
            settingsFile.replaceWithText(juce::JSON::toString(obj.get()));
        }
        
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
    };
    
    std::unique_ptr<MainWindow> mainWindow;
};

// Application entry point
START_JUCE_APPLICATION(MIDIXplorerStandaloneApp)
