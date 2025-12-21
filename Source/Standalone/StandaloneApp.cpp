#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Plugin/MIDIScalePlugin.h"
#include "../Plugin/PluginEditor.h"
#include "PianoSynthesizer.h"
#include "LicenseManager.h"
#include "ActivationDialog.h"

/**
 * Overlay component that blocks interaction when trial expires
 */
class TrialExpiredOverlay : public juce::Component
{
public:
    TrialExpiredOverlay()
    {
        setAlwaysOnTop(true);
        setInterceptsMouseClicks(true, true);

        titleLabel.setText("Trial Expired", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(28.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        titleLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(titleLabel);

        messageLabel.setText("Your 14-day free trial has expired.\nPlease activate a license to continue using MIDI Xplorer.",
                            juce::dontSendNotification);
        messageLabel.setFont(juce::Font(16.0f));
        messageLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.8f));
        messageLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(messageLabel);

        activateButton.setButtonText("Activate License");
        activateButton.setColour(juce::TextButton::buttonColourId, MIDIXplorerLookAndFeel::accentPrimary);
        activateButton.onClick = [this]() {
            ActivationDialog::showActivationDialog(this, [](bool) {});
        };
        addAndMakeVisible(activateButton);

        purchaseButton.setButtonText("Purchase License");
        purchaseButton.setColour(juce::TextButton::buttonColourId, MIDIXplorerLookAndFeel::colorSuccess);
        purchaseButton.onClick = []() {
            juce::URL("https://midixplorer.com/purchase").launchInDefaultBrowser();
        };
        addAndMakeVisible(purchaseButton);
    }

    void paint(juce::Graphics& g) override
    {
        // Dark semi-transparent overlay
        g.fillAll(MIDIXplorerLookAndFeel::bgPrimary.withAlpha(0.9f));

        // Central panel
        auto bounds = getLocalBounds();
        auto panelBounds = bounds.reduced(bounds.getWidth() / 4, bounds.getHeight() / 4);

        g.setColour(MIDIXplorerLookAndFeel::bgCard);
        g.fillRoundedRectangle(panelBounds.toFloat(), 15.0f);

        g.setColour(MIDIXplorerLookAndFeel::accentPrimary);
        g.drawRoundedRectangle(panelBounds.toFloat(), 15.0f, 2.0f);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        auto panelBounds = bounds.reduced(bounds.getWidth() / 4, bounds.getHeight() / 4);
        auto content = panelBounds.reduced(30);

        titleLabel.setBounds(content.removeFromTop(50));
        content.removeFromTop(20);
        messageLabel.setBounds(content.removeFromTop(60));
        content.removeFromTop(30);

        auto buttonArea = content.removeFromTop(45);
        auto buttonWidth = 180;
        auto totalWidth = buttonWidth * 2 + 20;
        auto startX = (buttonArea.getWidth() - totalWidth) / 2;

        activateButton.setBounds(buttonArea.getX() + startX, buttonArea.getY(), buttonWidth, 45);
        purchaseButton.setBounds(buttonArea.getX() + startX + buttonWidth + 20, buttonArea.getY(), buttonWidth, 45);
    }

private:
    juce::Label titleLabel;
    juce::Label messageLabel;
    juce::TextButton activateButton;
    juce::TextButton purchaseButton;
};

/**
 * Audio Settings Component with device selector and volume control
 */
class AudioSettingsComponent : public juce::Component
{
public:
    AudioSettingsComponent(juce::AudioDeviceManager& adm, std::function<void(float)> volumeCallback, float initialVolume)
        : deviceManager(adm), onVolumeChange(volumeCallback)
    {
        // Piano Volume section
        volumeLabel.setText("Piano Instrument Volume:", juce::dontSendNotification);
        volumeLabel.setFont(juce::Font(juce::FontOptions(14.0f).withStyle("Bold")));
        volumeLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(volumeLabel);

        volumeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        volumeSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 25);
        volumeSlider.setRange(0.0, 1.0, 0.01);
        volumeSlider.setValue(initialVolume);
        volumeSlider.setColour(juce::Slider::backgroundColourId, juce::Colour(0xff3a3a3a));
        volumeSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xff5ba8a0));
        volumeSlider.setColour(juce::Slider::thumbColourId, juce::Colours::white);
        volumeSlider.onValueChange = [this]() {
            if (onVolumeChange)
                onVolumeChange((float)volumeSlider.getValue());
        };
        addAndMakeVisible(volumeSlider);

        // Audio device selector
        deviceSelector = std::make_unique<juce::AudioDeviceSelectorComponent>(
            deviceManager, 0, 0, 0, 2, false, false, true, false);
        addAndMakeVisible(*deviceSelector);

        setSize(500, 520);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff2a2a2a));

        // Separator line
        g.setColour(juce::Colour(0xff4a4a4a));
        g.drawHorizontalLine(55, 10, getWidth() - 10);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10);

        // Volume section at top
        auto volumeArea = bounds.removeFromTop(45);
        volumeLabel.setBounds(volumeArea.removeFromTop(20));
        volumeSlider.setBounds(volumeArea);

        bounds.removeFromTop(20); // Spacing after separator

        // Device selector takes the rest
        if (deviceSelector)
            deviceSelector->setBounds(bounds);
    }

private:
    juce::AudioDeviceManager& deviceManager;
    std::function<void(float)> onVolumeChange;

    juce::Label volumeLabel;
    juce::Slider volumeSlider;
    std::unique_ptr<juce::AudioDeviceSelectorComponent> deviceSelector;
};

/**
 * Trial status bar shown at top of window
 */
class TrialStatusBar : public juce::Component
{
public:
    TrialStatusBar()
    {
        statusLabel.setFont(juce::Font(13.0f));
        statusLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(statusLabel);

        activateButton.setButtonText("Activate Now");
        activateButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff4a9eff));
        activateButton.onClick = [this]() {
            ActivationDialog::showActivationDialog(this, [](bool) {});
        };
        addAndMakeVisible(activateButton);

        updateStatus();
    }

    void updateStatus()
    {
        auto& license = LicenseManager::getInstance();

        if (license.isLicenseValid())
        {
            setVisible(false);
            isExpired = false;
        }
        else if (license.isInTrialPeriod())
        {
            int days = license.getTrialDaysRemaining();
            statusLabel.setText("Trial: " + juce::String(days) + " day" + (days != 1 ? "s" : "") + " remaining",
                               juce::dontSendNotification);

            if (days <= 3) {
                statusLabel.setColour(juce::Label::textColourId, juce::Colours::white);
                barColour = juce::Colour(0xffff8833);  // Orange
            } else {
                statusLabel.setColour(juce::Label::textColourId, juce::Colours::white);
                barColour = juce::Colour(0xff3366aa);  // Blue
            }

            setVisible(true);
            isExpired = false;
        }
        else
        {
            // Trial expired - show red bar
            statusLabel.setText("Trial Expired - Audio preview disabled. Please activate a license.",
                               juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, juce::Colours::white);
            barColour = juce::Colour(0xffcc3333);  // Red
            setVisible(true);
            isExpired = true;
        }
        repaint();
    }

    bool isLicenseExpired() const { return isExpired; }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(barColour);
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.drawLine(0, static_cast<float>(getHeight()) - 0.5f, static_cast<float>(getWidth()), static_cast<float>(getHeight()) - 0.5f);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10, 5);
        activateButton.setBounds(bounds.removeFromRight(120));
        bounds.removeFromRight(10);
        statusLabel.setBounds(bounds);
    }

private:
    juce::Label statusLabel;
    juce::TextButton activateButton;
    juce::Colour barColour { 0xff3366aa };  // Default blue
    bool isExpired = false;
};

/**
 * Standalone MIDI Xplorer Application
 * Full-featured MIDI file browser and player with built-in piano synthesizer
 * Features 14-day free trial and license activation system
 */
class MIDIXplorerStandaloneApp : public juce::JUCEApplication,
                                  private juce::Timer,
                                  private LicenseManager::Listener
{
public:
    MIDIXplorerStandaloneApp() = default;

    const juce::String getApplicationName() override { return "MIDI Xplorer"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String&) override
    {
        // Register as license listener
        getLicenseManager().addListener(this);

        // Initialize and check license/trial status
        checkLicenseAndLaunch();
    }

    // Helper to avoid ambiguity with JUCEApplication::getInstance()
    static LicenseManager& getLicenseManager()
    {
        return LicenseManager::getInstance();
    }

    void checkLicenseAndLaunch()
    {
        auto& licenseManager = getLicenseManager();
        juce::String savedKey = licenseManager.loadLicenseKey();

        if (savedKey.isNotEmpty())
        {
            // Has saved license - validate it
            licenseManager.validateLicense([this](LicenseManager::LicenseStatus status,
                                                   const LicenseManager::LicenseInfo& info)
            {
                if (status == LicenseManager::LicenseStatus::Valid)
                {
                    // License valid - launch fully
                    launchMainWindow(false);
                    getLicenseManager().startPeriodicValidation(3600);
                }
                else if (status == LicenseManager::LicenseStatus::NetworkError && info.isValid)
                {
                    // Offline but previously validated
                    launchMainWindow(false);
                }
                else
                {
                    // License invalid - check trial
                    checkTrialAndLaunch();
                }
            });
        }
        else
        {
            // No license - check/start trial
            checkTrialAndLaunch();
        }
    }

    void checkTrialAndLaunch()
    {
        auto& licenseManager = getLicenseManager();
        licenseManager.initializeTrial();
        licenseManager.checkTrialStatus();

        if (licenseManager.isInTrialPeriod())
        {
            // Trial active - launch with trial bar
            launchMainWindow(true);
        }
        else
        {
            // Trial expired - launch with status bar (no blocking overlay)
            launchMainWindow(true);
            // Note: Trial bar will show red "expired" message automatically
        }
    }

    void showTrialExpiredOverlay()
    {
        // No longer show blocking overlay - the trial bar shows red message instead
    }

    void launchMainWindow(bool showTrialBar)
    {
        mainWindow = std::make_unique<MainWindow>(getApplicationName(), showTrialBar);
    }

    void licenseStatusChanged(LicenseManager::LicenseStatus status,
                              const LicenseManager::LicenseInfo& /*info*/) override
    {
        // Handle license status changes
        if (status == LicenseManager::LicenseStatus::Valid)
        {
            // License activated - hide overlay and trial bar
            if (mainWindow)
            {
                mainWindow->hideExpiredOverlay();
                mainWindow->hideTrialBar();
            }
        }
        else if (status == LicenseManager::LicenseStatus::TrialExpired)
        {
            // Trial expired - just update the bar (no overlay)
            // The MainWindow's licenseStatusChanged will handle showing the red bar
        }
    }

    void timerCallback() override
    {
        // Not used anymore
    }

    void shutdown() override
    {
        getLicenseManager().removeListener(this);
        getLicenseManager().stopPeriodicValidation();
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted(const juce::String&) override {}

private:
    class MainWindow : public juce::DocumentWindow,
                       public juce::MenuBarModel,
                       public LicenseManager::Listener
    {
    public:
        explicit MainWindow(const juce::String& name, bool /*showTrialBar*/)
            : DocumentWindow(name, juce::Colour(0xff1a1a1a), DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);

            // Set up menu bar (macOS uses native menu bar)
            #if JUCE_MAC
            juce::MenuBarModel::setMacMainMenu(this);
            #endif

            // Create main content wrapper
            contentWrapper = std::make_unique<juce::Component>();

            // Create the processor (full plugin)
            processor = std::make_unique<MIDIScaleDetector::MIDIScalePlugin>();
            processor->prepareToPlay(44100.0, 512);

            // Initialize audio with piano synth
            pianoSynth.prepareToPlay(44100.0, 512);

            // Set pointers in audio callback AFTER creating processor and pianoSynth
            audioCallback.setProcessor(processor.get());
            audioCallback.setPianoSynth(&pianoSynth);

            auto audioError = audioDeviceManager.initialiseWithDefaultDevices(0, 2);
            if (audioError.isEmpty()) {
                audioDeviceManager.addAudioCallback(&audioCallback);
            } else {
                DBG("Audio device error: " << audioError);
            }

            // Create the full plugin editor UI
            pluginEditor = processor->createEditor();

            // Pass audio device manager and volume callback to the editor for settings dialog
            if (auto* midiEditor = dynamic_cast<MIDIXplorerEditor*>(pluginEditor)) {
                midiEditor->setAudioDeviceManager(&audioDeviceManager);
                midiEditor->setVolumeCallback([this](float vol) {
                    masterVolumeSlider.setValue(vol, juce::dontSendNotification);
                    audioCallback.setMasterVolume(vol);
                }, (float)masterVolumeSlider.getValue());
            }

            contentWrapper->addAndMakeVisible(pluginEditor);

            // Create audio control bar
            setupAudioControlBar();

            // Set up content wrapper size (PluginEditor handles its own license bar)
            int editorWidth = pluginEditor->getWidth();
            int editorHeight = pluginEditor->getHeight() + audioControlBarHeight;
            contentWrapper->setSize(editorWidth, editorHeight);

            setContentOwned(contentWrapper.release(), true);

            #if JUCE_IOS || JUCE_ANDROID
            setFullScreen(true);
            #else
            setResizable(true, true);
            centreWithSize(getWidth(), getHeight());
            #endif

            setVisible(true);

            // Register for license updates
            LicenseManager::getInstance().addListener(this);
        }

        ~MainWindow() override
        {
            LicenseManager::getInstance().removeListener(this);
            #if JUCE_MAC
            juce::MenuBarModel::setMacMainMenu(nullptr);
            #endif
            audioDeviceManager.removeAudioCallback(&audioCallback);
            pluginEditor = nullptr;
            processor = nullptr;
        }

        void setupAudioControlBar()
        {
            // Audio settings button
            audioSettingsButton.setButtonText("Audio Settings");
            audioSettingsButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3a3a3a));
            audioSettingsButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            audioSettingsButton.onClick = [this]() { showAudioSettings(); };
            contentWrapper->addAndMakeVisible(audioSettingsButton);

            // Volume label
            volumeLabel.setText("Volume:", juce::dontSendNotification);
            volumeLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
            volumeLabel.setFont(juce::Font(juce::FontOptions(13.0f)));
            contentWrapper->addAndMakeVisible(volumeLabel);

            // Master volume slider
            masterVolumeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
            masterVolumeSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 45, 20);
            masterVolumeSlider.setRange(0.0, 1.0, 0.01);
            masterVolumeSlider.setValue(1.0);
            masterVolumeSlider.setColour(juce::Slider::backgroundColourId, juce::Colour(0xff3a3a3a));
            masterVolumeSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xff5588cc));
            masterVolumeSlider.setColour(juce::Slider::thumbColourId, juce::Colours::white);
            masterVolumeSlider.onValueChange = [this]() {
                audioCallback.setMasterVolume((float)masterVolumeSlider.getValue());
            };
            contentWrapper->addAndMakeVisible(masterVolumeSlider);

            // Audio device info label
            updateAudioDeviceLabel();
            audioDeviceLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
            audioDeviceLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
            audioDeviceLabel.setJustificationType(juce::Justification::centredRight);
            contentWrapper->addAndMakeVisible(audioDeviceLabel);
        }

        void updateAudioDeviceLabel()
        {
            if (auto* device = audioDeviceManager.getCurrentAudioDevice()) {
                juce::String info = device->getName() + " | " +
                                   juce::String((int)device->getCurrentSampleRate()) + " Hz | " +
                                   juce::String(device->getCurrentBufferSizeSamples()) + " samples";
                audioDeviceLabel.setText(info, juce::dontSendNotification);
            } else {
                audioDeviceLabel.setText("No audio device", juce::dontSendNotification);
            }
        }

        void resized() override
        {
            DocumentWindow::resized();

            if (auto* content = getContentComponent())
            {
                auto bounds = content->getLocalBounds();

                // Audio control bar at bottom
                auto audioBar = bounds.removeFromBottom(audioControlBarHeight);
                audioBar = audioBar.reduced(8, 4);

                audioSettingsButton.setBounds(audioBar.removeFromLeft(110));
                audioBar.removeFromLeft(15);
                volumeLabel.setBounds(audioBar.removeFromLeft(55));
                masterVolumeSlider.setBounds(audioBar.removeFromLeft(140));
                audioBar.removeFromLeft(15);
                audioDeviceLabel.setBounds(audioBar);

                if (pluginEditor)
                    pluginEditor->setBounds(bounds);
            }
        }

        void showExpiredOverlay()
        {
            // No longer used - PluginEditor shows red status bar
        }

        void hideExpiredOverlay()
        {
            // No longer used
        }

        void hideTrialBar()
        {
            // No longer used - PluginEditor handles license status bar
        }

        void licenseStatusChanged(LicenseManager::LicenseStatus /*status*/,
                                  const LicenseManager::LicenseInfo& /*info*/) override
        {
            // PluginEditor handles all license status display
            // This callback is still registered to ensure license updates are processed
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

        // MenuBarModel implementation
        juce::StringArray getMenuBarNames() override
        {
            return { "File", "Settings", "Help" };
        }

        juce::PopupMenu getMenuForIndex(int menuIndex, const juce::String& /*menuName*/) override
        {
            juce::PopupMenu menu;

            if (menuIndex == 0) // File menu
            {
                menu.addItem(1, "Open MIDI File...");
                menu.addSeparator();
                #if ! JUCE_MAC
                menu.addItem(2, "Quit");
                #endif
            }
            else if (menuIndex == 1) // Settings menu
            {
                menu.addItem(10, "Audio Settings...");
                menu.addSeparator();
                menu.addItem(11, "License Management...");
                menu.addSeparator();
                menu.addItem(12, "Check For Updates...");
                menu.addSeparator();
                menu.addItem(13, "About MIDI Xplorer");
            }
            else if (menuIndex == 2) // Help menu
            {
                menu.addItem(20, "Getting Started");
                menu.addItem(21, "Keyboard Shortcuts");
                menu.addItem(22, "Understanding Scale Detection");
                menu.addItem(23, "MIDI File Management");
                menu.addSeparator();
                menu.addItem(24, "Online Documentation...");
                menu.addItem(25, "Report an Issue...");
                menu.addSeparator();
                menu.addItem(26, "Purchase License...");
            }

            return menu;
        }

        void menuItemSelected(int menuItemID, int /*topLevelMenuIndex*/) override
        {
            if (menuItemID == 1) // Open MIDI File
            {
                // TODO: Implement file open
            }
            else if (menuItemID == 2) // Quit
            {
                JUCEApplication::getInstance()->systemRequestedQuit();
            }
            else if (menuItemID == 10) // Audio Settings
            {
                showAudioSettings();
            }
            else if (menuItemID == 11) // License Management
            {
                ActivationDialog::showActivationDialog(this, nullptr);
            }
            else if (menuItemID == 12) // Check For Updates
            {
                checkForUpdates();
            }
            else if (menuItemID == 13) // About
            {
                showAboutDialog();
            }
            else if (menuItemID == 20) // Getting Started
            {
                showHelpDialog("Getting Started");
            }
            else if (menuItemID == 21) // Keyboard Shortcuts
            {
                showHelpDialog("Keyboard Shortcuts");
            }
            else if (menuItemID == 22) // Scale Detection
            {
                showHelpDialog("Scale Detection");
            }
            else if (menuItemID == 23) // MIDI Management
            {
                showHelpDialog("MIDI Management");
            }
            else if (menuItemID == 24) // Online Documentation
            {
                juce::URL("https://midixplorer.com/docs").launchInDefaultBrowser();
            }
            else if (menuItemID == 25) // Report an Issue
            {
                juce::URL("https://midixplorer.com/support").launchInDefaultBrowser();
            }
            else if (menuItemID == 26) // Purchase License
            {
                juce::URL("https://midixplorer.com/purchase").launchInDefaultBrowser();
            }
        }

        void showAudioSettings()
        {
            float currentVolume = (float)masterVolumeSlider.getValue();

            auto* settingsComponent = new AudioSettingsComponent(
                audioDeviceManager,
                [this](float vol) {
                    masterVolumeSlider.setValue(vol, juce::sendNotification);
                    audioCallback.setMasterVolume(vol);
                },
                currentVolume);

            juce::DialogWindow::LaunchOptions options;
            options.content.setOwned(settingsComponent);
            options.dialogTitle = "Audio/MIDI Settings";
            options.dialogBackgroundColour = juce::Colour(0xff2a2a2a);
            options.escapeKeyTriggersCloseButton = true;
            options.useNativeTitleBar = true;
            options.resizable = false;

            // Use a callback to update label after dialog closes
            auto* window = options.launchAsync();
            if (window) {
                // Use a timer to periodically update the label while dialog is open
                juce::Timer::callAfterDelay(500, [this]() {
                    updateAudioDeviceLabel();
                });
            }
        }

        void showAboutDialog()
        {
            auto& license = LicenseManager::getInstance();
            auto info = license.getLicenseInfo();

            juce::String message;
            message += "Version: 1.0.0\n";
            message += "Build Date: " + juce::String(__DATE__) + "\n\n";
            message += "A powerful MIDI file browser and analyzer\n";
            message += "with built-in piano synthesizer.\n\n";

            if (license.isLicenseValid())
            {
                message += "License Status: ACTIVE\n";
                message += "Licensed to: " + info.customerName + "\n";
                message += "License type: " + info.licenseType + "\n";
            }
            else if (license.isInTrialPeriod())
            {
                int days = license.getTrialDaysRemaining();
                message += "License Status: TRIAL\n";
                message += "Days remaining: " + juce::String(days) + "\n";
            }
            else
            {
                message += "License Status: EXPIRED\n";
                message += "Please activate a license to continue.\n";
            }

            message += "\n© 2024 MIDI Xplorer. All rights reserved.";

            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "About MIDI Xplorer",
                message,
                "OK");
        }

        void checkForUpdates()
        {
            // Show checking message
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "Check For Updates",
                "Checking for updates...\n\nCurrent Version: 1.0.0",
                "OK");

            // Launch update check in browser (or could implement proper version API)
            juce::URL("https://midixplorer.com/updates?v=1.0.0").launchInDefaultBrowser();
        }

        void showHelpDialog(const juce::String& topic)
        {
            juce::String title;
            juce::String content;

            if (topic == "Getting Started")
            {
                title = "Getting Started with MIDI Xplorer";
                content = "Welcome to MIDI Xplorer!\n\n";
                content += "1. ADD A LIBRARY\n";
                content += "   Click the '+' button in the left sidebar to add a folder containing MIDI files.\n\n";
                content += "2. BROWSE YOUR FILES\n";
                content += "   Click on a library to see all MIDI files. Use the search box to filter.\n\n";
                content += "3. PREVIEW MIDI FILES\n";
                content += "   Select a file to see the piano roll. Press Space to play/stop.\n\n";
                content += "4. DRAG & DROP\n";
                content += "   Drag any MIDI file from the list directly into your DAW.\n\n";
                content += "5. FILTER BY KEY\n";
                content += "   Use the 'All Keys' dropdown to filter by detected musical key.\n\n";
                content += "6. FILTER BY TYPE\n";
                content += "   Use 'All Types' to filter between chords and single notes.";
            }
            else if (topic == "Keyboard Shortcuts")
            {
                title = "Keyboard Shortcuts";
                content = "PLAYBACK\n";
                content += "  Space       - Play/Stop current MIDI file\n";
                content += "  N           - Play next/random file\n";
                content += "  L           - Toggle loop mode\n\n";
                content += "NAVIGATION\n";
                content += "  Up/Down     - Navigate file list\n";
                content += "  Enter       - Load selected file\n";
                content += "  Cmd/Ctrl+F  - Focus search box\n\n";
                content += "VIEW\n";
                content += "  F           - Toggle fullscreen piano roll\n";
                content += "  Escape      - Exit fullscreen\n\n";
                content += "FILE MANAGEMENT\n";
                content += "  Cmd/Ctrl+O  - Open MIDI file\n";
                content += "  Star icon   - Toggle favorite";
            }
            else if (topic == "Scale Detection")
            {
                title = "Understanding Scale Detection";
                content = "MIDI Xplorer analyzes every note in your MIDI file to detect the most likely musical scale.\n\n";
                content += "HOW IT WORKS\n";
                content += "The analyzer counts notes in each pitch class and matches them against 25+ scale templates including:\n";
                content += "  • Major & Minor modes (Ionian, Dorian, Phrygian, etc.)\n";
                content += "  • Pentatonic scales\n";
                content += "  • Blues scales\n";
                content += "  • Exotic scales (Hungarian, Spanish, Arabic, etc.)\n\n";
                content += "RELATIVE KEY DISPLAY\n";
                content += "The relative key (e.g., 'C/Am') shows the parent major and relative minor, helping you mix tracks in the same key family.\n\n";
                content += "CHORD vs NOTE DETECTION\n";
                content += "Files are classified as containing chords (3+ simultaneous notes) or single notes (melodies), which you can filter using the 'All Types' dropdown.";
            }
            else if (topic == "MIDI Management")
            {
                title = "MIDI File Management";
                content = "ORGANIZING YOUR LIBRARY\n";
                content += "Add multiple library folders to organize MIDI files by project, genre, or source.\n\n";
                content += "FAVORITES\n";
                content += "Click the star icon next to any file to mark it as a favorite. Access all favorites from the 'Favorites' section.\n\n";
                content += "RECENTLY PLAYED\n";
                content += "Quick access to files you've recently previewed.\n\n";
                content += "SEARCH & FILTER\n";
                content += "  • Search by filename, key, or instrument\n";
                content += "  • Filter by musical key (All Keys dropdown)\n";
                content += "  • Filter by content type (Chords/Notes)\n";
                content += "  • Sort by scale, duration, or name\n\n";
                content += "DRAG TO DAW\n";
                content += "Simply drag any file from the list into your DAW's arrangement or MIDI track. The file will be imported at the current position.";
            }
            else
            {
                title = "Help";
                content = "Select a help topic from the Help menu.";
            }

            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                title,
                content,
                "OK");
        }

        void showLicenseStatus()
        {
            auto& license = LicenseManager::getInstance();
            auto info = license.getLicenseInfo();
            auto trial = license.getTrialInfo();

            juce::String message;

            if (license.isLicenseValid())
            {
                message = "License Status: ACTIVE\n\n";
                message += "Customer: " + info.customerName + "\n";
                message += "Email: " + info.email + "\n";
                message += "Type: " + info.licenseType + "\n";
                message += "Activations: " + juce::String(info.currentActivations) +
                           "/" + juce::String(info.maxActivations) + "\n";

                if (info.expiryDate.isNotEmpty())
                    message += "Expires: " + info.expiryDate + "\n";
                else
                    message += "Expires: Never\n";
            }
            else if (license.isInTrialPeriod())
            {
                int days = license.getTrialDaysRemaining();
                message = "License Status: FREE TRIAL\n\n";
                message += "Days Remaining: " + juce::String(days) + "\n";
                message += "Trial Started: " + trial.firstLaunchDate.toString(true, false) + "\n\n";
                message += "Activate a license to unlock unlimited usage.";
            }
            else
            {
                message = "License Status: TRIAL EXPIRED\n\n";
                message += "Your 14-day free trial has ended.\n";
                message += "Please activate a license to continue using MIDI Xplorer.";
            }

            juce::AlertWindow::showMessageBoxAsync(
                license.isLicenseValid() ? juce::AlertWindow::InfoIcon : juce::AlertWindow::WarningIcon,
                "License Status",
                message,
                "OK");
        }

    private:
        std::unique_ptr<MIDIScaleDetector::MIDIScalePlugin> processor;
        juce::AudioDeviceManager audioDeviceManager;
        PianoSynthesizer pianoSynth;
        juce::AudioProcessorEditor* pluginEditor = nullptr;
        std::unique_ptr<juce::Component> contentWrapper;

        // Audio control bar components
        static constexpr int audioControlBarHeight = 36;
        juce::TextButton audioSettingsButton;
        juce::Slider masterVolumeSlider;
        juce::Label volumeLabel;
        juce::Label audioDeviceLabel;

        // Audio callback that combines processor output with piano synth
        class AudioCallback : public juce::AudioIODeviceCallback
        {
        public:
            AudioCallback()
                : processor(nullptr), pianoSynth(nullptr), masterVolume(1.0f) {}

            void setProcessor(MIDIScaleDetector::MIDIScalePlugin* proc) {
                processor = proc;
            }

            void setPianoSynth(PianoSynthesizer* synth) {
                pianoSynth = synth;
            }

            void setMasterVolume(float volume) {
                masterVolume = juce::jlimit(0.0f, 1.0f, volume);
            }

            float getMasterVolume() const {
                return masterVolume;
            }

            void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                                 int numInputChannels,
                                                 float* const* outputChannelData,
                                                 int numOutputChannels,
                                                 int numSamples,
                                                 const juce::AudioIODeviceCallbackContext& context) override
            {
                juce::ignoreUnused(inputChannelData, numInputChannels, context);

                // Create buffers
                juce::AudioBuffer<float> buffer(outputChannelData, numOutputChannels, numSamples);
                juce::MidiBuffer midiMessages;

                // Check license status - mute audio if expired
                bool isLicenseValid = LicenseManager::getInstance().isLicenseValid();
                bool isTrialActive = LicenseManager::getInstance().isInTrialPeriod();
                bool licenseExpired = !isLicenseValid && !isTrialActive;

                if (licenseExpired) {
                    buffer.clear();
                    if (pianoSynth) {
                        pianoSynth->allNotesOff();
                    }
                    return;
                }

                // Check if playback just stopped (to silence all notes)
                bool currentlyPlaying = processor && processor->isPlaybackPlaying();
                if (wasPlaying && !currentlyPlaying) {
                    // Playback just stopped - silence all notes immediately
                    if (pianoSynth) {
                        pianoSynth->allNotesOff();
                    }
                }
                wasPlaying = currentlyPlaying;

                // Get processor output (MIDI events)
                if (processor) {
                    processor->processBlock(buffer, midiMessages);
                }

                // Render MIDI through piano synth
                buffer.clear();
                if (pianoSynth) {
                    pianoSynth->processBlock(buffer, midiMessages);

                    // Apply master volume
                    buffer.applyGain(masterVolume);
                }
            }

            void audioDeviceAboutToStart(juce::AudioIODevice* device) override
            {
                if (pianoSynth && device) {
                    pianoSynth->prepareToPlay(device->getCurrentSampleRate(),
                                             device->getCurrentBufferSizeSamples());
                }
                if (processor && device) {
                    processor->prepareToPlay(device->getCurrentSampleRate(),
                                           device->getCurrentBufferSizeSamples());
                }
            }

            void audioDeviceStopped() override
            {
                if (processor) processor->releaseResources();
            }

        private:
            MIDIScaleDetector::MIDIScalePlugin* processor;
            PianoSynthesizer* pianoSynth;
            float masterVolume = 1.0f;
            bool wasPlaying = false;
        } audioCallback;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

    std::unique_ptr<MainWindow> mainWindow;
};

// Application entry point
START_JUCE_APPLICATION(MIDIXplorerStandaloneApp)
