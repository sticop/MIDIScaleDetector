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
        activateButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff4a9eff));
        activateButton.onClick = [this]() {
            ActivationDialog::showActivationDialog(this, [](bool) {});
        };
        addAndMakeVisible(activateButton);

        purchaseButton.setButtonText("Purchase License");
        purchaseButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2ecc71));
        purchaseButton.onClick = []() {
            juce::URL("https://reliablehandy.ca/midixplorer/purchase").launchInDefaultBrowser();
        };
        addAndMakeVisible(purchaseButton);
    }

    void paint(juce::Graphics& g) override
    {
        // Dark semi-transparent overlay
        g.fillAll(juce::Colour(0xe0101020));

        // Central panel
        auto bounds = getLocalBounds();
        auto panelBounds = bounds.reduced(bounds.getWidth() / 4, bounds.getHeight() / 4);

        g.setColour(juce::Colour(0xff1a1a2e));
        g.fillRoundedRectangle(panelBounds.toFloat(), 15.0f);

        g.setColour(juce::Colour(0xff4a9eff));
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
            // Trial expired - show overlay
            if (mainWindow)
            {
                mainWindow->showExpiredOverlay();
            }
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
        explicit MainWindow(const juce::String& name, bool showTrialBar)
            : DocumentWindow(name, juce::Colour(0xff1a1a1a), DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);

            // Set up menu bar (macOS uses native menu bar)
            #if JUCE_MAC
            juce::MenuBarModel::setMacMainMenu(this);
            #endif

            // Create main content wrapper
            contentWrapper = std::make_unique<juce::Component>();

            // Create trial status bar
            trialBar = std::make_unique<TrialStatusBar>();
            if (showTrialBar && LicenseManager::getInstance().isInTrialPeriod())
            {
                contentWrapper->addAndMakeVisible(trialBar.get());
            }

            // Create the processor (full plugin)
            processor = std::make_unique<MIDIScaleDetector::MIDIScalePlugin>();
            processor->prepareToPlay(44100.0, 512);

            // Initialize audio with piano synth
            pianoSynth.prepareToPlay(44100.0, 512);

            auto audioError = audioDeviceManager.initialiseWithDefaultDevices(0, 2);
            if (audioError.isEmpty()) {
                audioDeviceManager.addAudioCallback(&audioCallback);
            }

            // Create the full plugin editor UI
            pluginEditor = processor->createEditor();
            contentWrapper->addAndMakeVisible(pluginEditor);

            // Set up content wrapper size
            int editorWidth = pluginEditor->getWidth();
            int editorHeight = pluginEditor->getHeight();
            int trialBarHeight = (showTrialBar && LicenseManager::getInstance().isInTrialPeriod()) ? 35 : 0;
            contentWrapper->setSize(editorWidth, editorHeight + trialBarHeight);

            setContentOwned(contentWrapper.release(), true);

            // Create expired overlay (hidden by default)
            expiredOverlay = std::make_unique<TrialExpiredOverlay>();
            expiredOverlay->setVisible(false);
            addChildComponent(expiredOverlay.get());

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

        void resized() override
        {
            DocumentWindow::resized();

            if (auto* content = getContentComponent())
            {
                auto bounds = content->getLocalBounds();
                int trialBarHeight = (trialBar && trialBar->isVisible()) ? 35 : 0;

                if (trialBar)
                    trialBar->setBounds(bounds.removeFromTop(trialBarHeight));

                if (pluginEditor)
                    pluginEditor->setBounds(bounds);
            }

            if (expiredOverlay)
            {
                expiredOverlay->setBounds(getLocalBounds());
            }
        }

        void showExpiredOverlay()
        {
            if (expiredOverlay)
            {
                expiredOverlay->setVisible(true);
                expiredOverlay->toFront(true);
                expiredOverlay->setBounds(getLocalBounds());
            }
        }

        void hideExpiredOverlay()
        {
            if (expiredOverlay)
            {
                expiredOverlay->setVisible(false);
            }
        }

        void hideTrialBar()
        {
            if (trialBar)
            {
                trialBar->setVisible(false);
                resized();
            }
        }

        void licenseStatusChanged(LicenseManager::LicenseStatus status,
                                  const LicenseManager::LicenseInfo& /*info*/) override
        {
            if (status == LicenseManager::LicenseStatus::Valid)
            {
                hideExpiredOverlay();
                hideTrialBar();
            }
            else if (status == LicenseManager::LicenseStatus::Trial)
            {
                if (trialBar) {
                    trialBar->updateStatus();
                    trialBar->setVisible(true);
                    resized();
                }
            }
            else if (status == LicenseManager::LicenseStatus::TrialExpired ||
                     status == LicenseManager::LicenseStatus::Expired ||
                     status == LicenseManager::LicenseStatus::Invalid)
            {
                // Show red status bar instead of blocking overlay
                if (trialBar) {
                    trialBar->updateStatus();
                    trialBar->setVisible(true);
                    resized();
                }
            }
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
                menu.addItem(11, "License...");
                menu.addItem(12, "Enter License Key...");
            }
            else if (menuIndex == 2) // Help menu
            {
                menu.addItem(20, "About MIDI Xplorer");
                menu.addItem(21, "License Status");
                menu.addSeparator();
                menu.addItem(22, "Purchase License...");
                menu.addItem(23, "Online Help...");
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
            else if (menuItemID == 11) // License
            {
                showLicenseStatus();
            }
            else if (menuItemID == 12) // Enter License Key
            {
                ActivationDialog::showActivationDialog(this, nullptr);
            }
            else if (menuItemID == 20) // About
            {
                showAboutDialog();
            }
            else if (menuItemID == 21) // License Status
            {
                showLicenseStatus();
            }
            else if (menuItemID == 22) // Purchase License
            {
                juce::URL("https://reliablehandy.ca/midixplorer/purchase").launchInDefaultBrowser();
            }
            else if (menuItemID == 23) // Online Help
            {
                juce::URL("https://reliablehandy.ca/midixplorer/help").launchInDefaultBrowser();
            }
        }

        void showAudioSettings()
        {
            auto* selector = new juce::AudioDeviceSelectorComponent(
                audioDeviceManager, 0, 0, 0, 2, false, false, true, false);
            selector->setSize(500, 400);

            juce::DialogWindow::LaunchOptions options;
            options.content.setOwned(selector);
            options.dialogTitle = "Audio Settings";
            options.dialogBackgroundColour = juce::Colour(0xff2a2a2a);
            options.escapeKeyTriggersCloseButton = true;
            options.useNativeTitleBar = true;
            options.resizable = false;
            options.launchAsync();
        }

        void showAboutDialog()
        {
            auto& license = LicenseManager::getInstance();
            auto info = license.getLicenseInfo();

            juce::String message = "MIDI Xplorer v1.0.0\n\n";
            message += "A powerful MIDI file browser and analyzer\n";
            message += "with built-in piano synthesizer.\n\n";

            if (license.isLicenseValid())
            {
                message += "Licensed to: " + info.customerName + "\n";
                message += "License type: " + info.licenseType + "\n";
            }
            else if (license.isInTrialPeriod())
            {
                int days = license.getTrialDaysRemaining();
                message += "Trial Version - " + juce::String(days) + " days remaining\n";
            }
            else
            {
                message += "Trial Expired - Please activate license\n";
            }

            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "About MIDI Xplorer",
                message,
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
        std::unique_ptr<TrialStatusBar> trialBar;
        std::unique_ptr<TrialExpiredOverlay> expiredOverlay;

        // Audio callback that combines processor output with piano synth
        class AudioCallback : public juce::AudioIODeviceCallback
        {
        public:
            AudioCallback(MIDIScaleDetector::MIDIScalePlugin* proc, PianoSynthesizer* synth)
                : processor(proc), pianoSynth(synth) {}

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
                bool licenseExpired = !LicenseManager::getInstance().isLicenseValid() &&
                                     !LicenseManager::getInstance().isInTrialPeriod();
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
                // PianoSynthesizer doesn't have releaseResources
                if (processor) processor->releaseResources();
            }

        private:
            MIDIScaleDetector::MIDIScalePlugin* processor;
            PianoSynthesizer* pianoSynth;
            bool wasPlaying = false;
        } audioCallback{processor.get(), &pianoSynth};

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

    std::unique_ptr<MainWindow> mainWindow;
};

// Application entry point
START_JUCE_APPLICATION(MIDIXplorerStandaloneApp)
