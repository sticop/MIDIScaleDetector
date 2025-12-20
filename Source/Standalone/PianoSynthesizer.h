#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>

/**
 * High-quality piano synthesizer using additive synthesis with realistic envelope and harmonics
 */
class PianoVoice : public juce::SynthesiserVoice
{
public:
    PianoVoice() = default;

    bool canPlaySound(juce::SynthesiserSound*) override
    {
        // Accept any sound - we'll forward declare PianoSound check isn't needed
        // since we only add PianoSound to the synth
        return true;
    }

    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override
    {
        noteNumber = midiNoteNumber;
        level = velocity * 0.8f;

        // Calculate frequency from MIDI note
        frequency = 440.0 * std::pow(2.0, (midiNoteNumber - 69) / 12.0);

        // Reset phases for all harmonics
        for (int i = 0; i < NUM_HARMONICS; i++) {
            phases[i] = 0.0;
        }

        // Set envelope parameters based on note (lower notes have longer decay)
        double noteRatio = (128.0 - midiNoteNumber) / 128.0;
        attackTime = 0.002;  // 2ms attack
        decayTime = 0.5 + noteRatio * 2.0;  // 0.5s to 2.5s decay
        sustainLevel = 0.3f - (float)noteRatio * 0.2f;  // Lower sustain for low notes
        releaseTime = 0.3 + noteRatio * 0.5;  // 0.3s to 0.8s release

        envelopePhase = EnvelopePhase::Attack;
        envelopeLevel = 0.0f;
        envelopeTime = 0.0;

        // Harmonic amplitudes for piano-like timbre
        // Lower notes have more harmonics, higher notes have fewer
        double brightnessRatio = midiNoteNumber / 127.0;
        harmonicAmplitudes[0] = 1.0f;  // Fundamental
        harmonicAmplitudes[1] = 0.6f * (1.0f - (float)brightnessRatio * 0.3f);  // 2nd
        harmonicAmplitudes[2] = 0.4f * (1.0f - (float)brightnessRatio * 0.4f);  // 3rd
        harmonicAmplitudes[3] = 0.25f * (1.0f - (float)brightnessRatio * 0.5f); // 4th
        harmonicAmplitudes[4] = 0.15f * (1.0f - (float)brightnessRatio * 0.6f); // 5th
        harmonicAmplitudes[5] = 0.1f * (1.0f - (float)brightnessRatio * 0.7f);  // 6th
        harmonicAmplitudes[6] = 0.05f * (1.0f - (float)brightnessRatio * 0.8f); // 7th
        harmonicAmplitudes[7] = 0.02f * (1.0f - (float)brightnessRatio * 0.9f); // 8th

        // Normalize
        float total = 0.0f;
        for (int i = 0; i < NUM_HARMONICS; i++) {
            if (harmonicAmplitudes[i] < 0) harmonicAmplitudes[i] = 0;
            total += harmonicAmplitudes[i];
        }
        if (total > 0) {
            for (int i = 0; i < NUM_HARMONICS; i++) {
                harmonicAmplitudes[i] /= total;
            }
        }
    }

    void stopNote(float, bool allowTailOff) override
    {
        if (allowTailOff) {
            if (envelopePhase != EnvelopePhase::Off) {
                envelopePhase = EnvelopePhase::Release;
                envelopeTime = 0.0;
            }
        } else {
            envelopePhase = EnvelopePhase::Off;
            envelopeLevel = 0.0f;
            clearCurrentNote();
        }
    }

    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}

    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override
    {
        if (envelopePhase == EnvelopePhase::Off)
            return;

        double sampleRate = getSampleRate();
        double timeIncrement = 1.0 / sampleRate;

        for (int sample = 0; sample < numSamples; ++sample) {
            // Calculate envelope
            updateEnvelope(timeIncrement);

            if (envelopePhase == EnvelopePhase::Off) {
                clearCurrentNote();
                break;
            }

            // Generate sample using additive synthesis with harmonics
            float sampleValue = 0.0f;

            for (int h = 0; h < NUM_HARMONICS; h++) {
                int harmonicNumber = h + 1;
                double harmonicFreq = frequency * harmonicNumber;

                // Skip harmonics above Nyquist
                if (harmonicFreq > sampleRate * 0.45) continue;

                // Slight detuning for richness (like real piano strings)
                double detune = 1.0 + (harmonicNumber - 1) * 0.0001 * (1.0 + std::sin(phases[h] * 0.1));

                sampleValue += harmonicAmplitudes[h] * (float)std::sin(phases[h]);

                // Update phase
                phases[h] += 2.0 * juce::MathConstants<double>::pi * harmonicFreq * detune / sampleRate;
                if (phases[h] > 2.0 * juce::MathConstants<double>::pi) {
                    phases[h] -= 2.0 * juce::MathConstants<double>::pi;
                }
            }

            // Apply envelope and velocity
            sampleValue *= envelopeLevel * level;

            // Soft clipping for warmth
            sampleValue = std::tanh(sampleValue * 1.5f) / 1.5f;

            // Output to all channels
            for (int channel = 0; channel < outputBuffer.getNumChannels(); ++channel) {
                outputBuffer.addSample(channel, startSample + sample, sampleValue);
            }
        }
    }

private:
    static constexpr int NUM_HARMONICS = 8;

    enum class EnvelopePhase { Attack, Decay, Sustain, Release, Off };

    int noteNumber = 0;
    double frequency = 440.0;
    float level = 0.0f;

    double phases[NUM_HARMONICS] = {};
    float harmonicAmplitudes[NUM_HARMONICS] = {};

    EnvelopePhase envelopePhase = EnvelopePhase::Off;
    float envelopeLevel = 0.0f;
    double envelopeTime = 0.0;

    double attackTime = 0.002;
    double decayTime = 1.0;
    float sustainLevel = 0.3f;
    double releaseTime = 0.3;

    void updateEnvelope(double timeIncrement)
    {
        envelopeTime += timeIncrement;

        switch (envelopePhase) {
            case EnvelopePhase::Attack:
                envelopeLevel = (float)(envelopeTime / attackTime);
                if (envelopeTime >= attackTime) {
                    envelopeLevel = 1.0f;
                    envelopePhase = EnvelopePhase::Decay;
                    envelopeTime = 0.0;
                }
                break;

            case EnvelopePhase::Decay:
                envelopeLevel = 1.0f - (1.0f - sustainLevel) * (float)(envelopeTime / decayTime);
                if (envelopeTime >= decayTime) {
                    envelopeLevel = sustainLevel;
                    envelopePhase = EnvelopePhase::Sustain;
                }
                break;

            case EnvelopePhase::Sustain:
                // Gradual decay in sustain phase (like real piano)
                envelopeLevel = sustainLevel * (float)std::exp(-envelopeTime * 0.5);
                if (envelopeLevel < 0.001f) {
                    envelopePhase = EnvelopePhase::Off;
                    envelopeLevel = 0.0f;
                }
                break;

            case EnvelopePhase::Release:
                envelopeLevel *= (float)std::exp(-timeIncrement / releaseTime * 5.0);
                if (envelopeLevel < 0.001f) {
                    envelopePhase = EnvelopePhase::Off;
                    envelopeLevel = 0.0f;
                }
                break;

            case EnvelopePhase::Off:
                break;
        }
    }
};

class PianoSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

/**
 * Piano Synthesizer - wraps JUCE Synthesiser with piano voices
 */
class PianoSynthesizer
{
public:
    PianoSynthesizer()
    {
        // Add piano sound
        synth.addSound(new PianoSound());

        // Add voices for polyphony (16 voice polyphony)
        for (int i = 0; i < 16; ++i) {
            synth.addVoice(new PianoVoice());
        }
    }

    void prepareToPlay(double sampleRate, int)
    {
        synth.setCurrentPlaybackSampleRate(sampleRate);
    }

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
    {
        synth.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());
        
        // Apply master gain to ensure audible output
        buffer.applyGain(2.0f);
    }

    void allNotesOff()
    {
        synth.allNotesOff(0, true);
    }

    void noteOn(int channel, int noteNumber, float velocity)
    {
        synth.noteOn(channel, noteNumber, velocity);
    }

    void noteOff(int channel, int noteNumber)
    {
        synth.noteOff(channel, noteNumber, 0.0f, true);
    }

private:
    juce::Synthesiser synth;
};
