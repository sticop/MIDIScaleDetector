#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <iostream>

// TinySoundFont implementation
#define TSF_IMPLEMENTATION
#include "tsf.h"

/**
 * SoundFont-based Piano Synthesizer using TinySoundFont library
 * Uses the SGM Piano HD SoundFont for realistic piano sound
 */
class PianoSynthesizer
{
public:
    PianoSynthesizer()
    {
        // Configure reverb for concert hall feel
        juce::Reverb::Parameters reverbParams;
        reverbParams.roomSize = 0.55f;     // Medium room
        reverbParams.damping = 0.4f;       // Warm damping
        reverbParams.wetLevel = 0.18f;     // Subtle reverb
        reverbParams.dryLevel = 0.82f;     // Keep most dry signal
        reverbParams.width = 1.0f;         // Full stereo width
        reverbParams.freezeMode = 0.0f;    // No freeze
        reverb.setParameters(reverbParams);
    }

    ~PianoSynthesizer()
    {
        if (soundFont != nullptr) {
            tsf_close(soundFont);
            soundFont = nullptr;
        }
    }

    bool loadSoundFont(const juce::String& path)
    {
        // Close existing soundfont if loaded
        if (soundFont != nullptr) {
            tsf_close(soundFont);
            soundFont = nullptr;
        }

        std::cerr << "[PianoSynth] Attempting to load SoundFont: " << path << std::endl;
        soundFont = tsf_load_filename(path.toRawUTF8());

        if (soundFont != nullptr) {
            tsf_set_output(soundFont, TSF_STEREO_INTERLEAVED, static_cast<int>(currentSampleRate), 0.0f);
            soundFontLoaded = true;
            std::cerr << "[PianoSynth] SoundFont loaded successfully!" << std::endl;
            // Log preset count
            int presetCount = tsf_get_presetcount(soundFont);
            std::cerr << "[PianoSynth] SoundFont has " << presetCount << " presets" << std::endl;
            if (presetCount > 0) {
                const char* presetName = tsf_get_presetname(soundFont, 0);
                std::cerr << "[PianoSynth] Preset 0 name: " << (presetName ? presetName : "(null)") << std::endl;
            }
            return true;
        }

        std::cerr << "[PianoSynth] FAILED to load SoundFont!" << std::endl;
        soundFontLoaded = false;
        return false;
    }

    bool isSoundFontLoaded() const { return soundFontLoaded; }

    void prepareToPlay(double sampleRate, int samplesPerBlock)
    {
        std::cerr << "[PianoSynth] prepareToPlay called with sampleRate=" << sampleRate << " bufferSize=" << samplesPerBlock << std::endl;
        currentSampleRate = sampleRate;

        if (soundFont != nullptr) {
            std::cerr << "[PianoSynth] Setting TSF output to sampleRate=" << static_cast<int>(sampleRate) << std::endl;
            tsf_set_output(soundFont, TSF_STEREO_INTERLEAVED, static_cast<int>(sampleRate), 0.0f);
        } else {
            std::cerr << "[PianoSynth] WARNING: SoundFont not loaded yet in prepareToPlay!" << std::endl;
        }

        // Prepare reverb
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sampleRate;
        spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
        spec.numChannels = 2;
        reverb.prepare(spec);
        reverb.reset();

        // Allocate render buffer
        renderBuffer.resize(static_cast<size_t>(samplesPerBlock) * 2);  // Stereo interleaved
    }

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
    {
        if (soundFont == nullptr) {
            // Fallback: silence if no soundfont
            static int noSfCounter = 0;
            if (noSfCounter++ % 1000 == 0) {
                std::cerr << "[PianoSynth] No SoundFont loaded! (warning #" << noSfCounter << ")" << std::endl;
            }
            buffer.clear();
            return;
        }

        int numSamples = buffer.getNumSamples();

        // Debug: show how many MIDI messages received
        static int callCount = 0;
        callCount++;
        if (!midiMessages.isEmpty()) {
            std::cerr << "[PianoSynth] processBlock call #" << callCount << " with " << midiMessages.getNumEvents() << " MIDI messages" << std::endl;
        }

        // Process MIDI messages
        for (const auto metadata : midiMessages) {
            auto msg = metadata.getMessage();

            if (msg.isNoteOn()) {
                std::cerr << "[PianoSynth] NoteOn: note=" << msg.getNoteNumber() << " vel=" << (int)msg.getVelocity() << std::endl;
                // Use preset 0 (first piano preset in the SF2)
                tsf_note_on(soundFont, 0, msg.getNoteNumber(), msg.getVelocity() / 127.0f);
            }
            else if (msg.isNoteOff()) {
                std::cerr << "[PianoSynth] NoteOff: note=" << msg.getNoteNumber() << std::endl;
                tsf_note_off(soundFont, 0, msg.getNoteNumber());
            }
            else if (msg.isAllNotesOff() || msg.isAllSoundOff()) {
                tsf_note_off_all(soundFont);
            }
            else if (msg.isController()) {
                // Handle sustain pedal
                if (msg.getControllerNumber() == 64) {
                    // Sustain pedal - TSF doesn't have direct support, but we can handle it
                }
            }
        }

        // Render audio from soundfont
        if (renderBuffer.size() < static_cast<size_t>(numSamples) * 2) {
            renderBuffer.resize(static_cast<size_t>(numSamples) * 2);
        }

        tsf_render_float(soundFont, renderBuffer.data(), numSamples, 0);

        // Check for audio output (debug)
        static int audioCheckCounter = 0;
        if (audioCheckCounter++ % 500 == 0) {
            float maxSample = 0.0f;
            for (int i = 0; i < numSamples * 2; ++i) {
                float absVal = std::abs(renderBuffer[static_cast<size_t>(i)]);
                if (absVal > maxSample) maxSample = absVal;
            }
            if (maxSample > 0.001f) {
                std::cerr << "[PianoSynth] Audio output level: " << maxSample << std::endl;
            }
        }

        // Convert from interleaved stereo to JUCE buffer
        float* leftChannel = buffer.getWritePointer(0);
        float* rightChannel = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;

        for (int i = 0; i < numSamples; ++i) {
            leftChannel[i] = renderBuffer[static_cast<size_t>(i) * 2];
            if (rightChannel != nullptr) {
                rightChannel[i] = renderBuffer[static_cast<size_t>(i) * 2 + 1];
            }
        }

        // Apply reverb for spaciousness
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        reverb.process(context);

        // Apply master gain
        buffer.applyGain(1.5f);
    }

    void allNotesOff()
    {
        if (soundFont != nullptr) {
            tsf_note_off_all(soundFont);
        }
    }

    void noteOn(int channel, int noteNumber, float velocity)
    {
        (void)channel;
        if (soundFont != nullptr) {
            tsf_note_on(soundFont, 0, noteNumber, velocity);
        }
    }

    void noteOff(int channel, int noteNumber)
    {
        (void)channel;
        if (soundFont != nullptr) {
            tsf_note_off(soundFont, 0, noteNumber);
        }
    }

private:
    tsf* soundFont = nullptr;
    bool soundFontLoaded = false;
    juce::dsp::Reverb reverb;
    double currentSampleRate = 44100.0;
    std::vector<float> renderBuffer;
};

