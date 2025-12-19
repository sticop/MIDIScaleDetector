#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Plugin/MIDIScalePlugin.h"
#include "../Plugin/PluginEditor.h"
#include "PianoSynthesizer.h"

/**
 * Standalone MIDI Xplorer Application
 * Full-featured MIDI file browser and player with built-in piano synthesizer
 * Uses the same UI and functionality as the plugin version
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
            auto* editorComponent = processor->createEditor();
            setContentOwned(editorComponent, true);

            #if JUCE_IOS || JUCE_ANDROID
            setFullScreen(true);
            #else
            setResizable(true, true);
            centreWithSize(getWidth(), getHeight());
            #endif

            setVisible(true);
        }

        ~MainWindow() override
        {
            audioDeviceManager.removeAudioCallback(&audioCallback);
            setContentOwned(nullptr, false);
            processor = nullptr;
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        std::unique_ptr<MIDIScaleDetector::MIDIScalePlugin> processor;
        juce::AudioDeviceManager audioDeviceManager;
        PianoSynthesizer pianoSynth;

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

                // Get processor output (MIDI events)
                if (processor) {
                    processor->processBlock(buffer, midiMessages);
                }

                // Render MIDI through piano synth
                buffer.clear();
                if (pianoSynth) {
                    pianoSynth->renderNextBlock(buffer, midiMessages, 0, numSamples);
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
                if (pianoSynth) pianoSynth->releaseResources();
                if (processor) processor->releaseResources();
            }

        private:
            MIDIScaleDetector::MIDIScalePlugin* processor;
            PianoSynthesizer* pianoSynth;
        } audioCallback{processor.get(), &pianoSynth};

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

    std::unique_ptr<MainWindow> mainWindow;
};

// Application entry point
START_JUCE_APPLICATION(MIDIXplorerStandaloneApp)
