#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "DSPUtils.h"
#include <vector>

class SamplePlayer
{
public:
    // Add playback mode enum
    enum class PlaybackMode {
        Polyphonic,    // Normal polyphonic mode (plays while held)
        Monophonic,    // Single voice, continues playing after release
        OneShot        // Multiple independent voices, each continues until stopped
    };

    SamplePlayer();
    ~SamplePlayer();

    void loadFile(const juce::File& file);
    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void releaseResources();
    void processBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    void handleMidiMessage(const juce::MidiMessage& message);
    void setPlaybackSpeed(float speed);
    void setLooping(bool shouldLoop);
    bool getLooping() const { return isLooping; }
    void setHoldMode(bool shouldHold);
    bool getHoldMode() const { return isHoldMode; }
    void setHoldPosition(double normalizedPosition);
    double getHoldPosition() const;
    bool isFileLoaded() const { return fileBuffer.getNumSamples() > 0; }
    double getLengthInSeconds() const;
    float getCurrentLevel() const { return currentLevel.load(); }
    double getCurrentPosition() const;
    void setEnabled(bool shouldBeEnabled) { isEnabled = shouldBeEnabled; }
    bool getEnabled() const { return isEnabled; }
    void stopAllVoices();

    // Replace trigger mode with playback mode
    void setPlaybackMode(PlaybackMode mode) { playbackMode = mode; }
    PlaybackMode getPlaybackMode() const { return playbackMode; }

private:
    struct Grain {
        double startPosition = 0.0;
        double currentPosition = 0.0;
        double grainLength = 0.0;
        double age = 0.0;
        bool isActive = false;
        float phase = 0.0f;  // Phase for window calculation
        
        // Phase alignment
        float initialPhase = 0.0f;
        float phaseIncrement = 0.0f;
    };

    struct Voice {
        bool isActive = false;
        double position = 0.0;
        double pitchRatio = 1.0;
        float velocity = 0.0f;
        int midiNote = -1;
        float lastOutputSample = 0.0f;
        
        std::vector<Grain> grains;
        float grainDuration = 0.1f;
        float grainOverlap = 0.5f;
        
        // DSP processing chain
        DSPUtils::Resampler resampler;
        DSPUtils::ButterworthFilter antiAliasFilter;
        DSPUtils::DCBlocker dcBlocker;
        DSPUtils::SoftClipper softClipper;
        
        struct Envelope {
            float attackTime = 0.01f;
            float decayTime = 0.1f;
            float sustainLevel = 0.7f;
            float releaseTime = 0.2f;
            float sampleRate = 44100.0f;
            
            enum class State {
                Idle,
                Attack,
                Decay,
                Sustain,
                Release
            };
            
            State state = State::Idle;
            float currentLevel = 0.0f;
            float currentTime = 0.0f;
            
            void setParameters(float attack, float decay, float sustain, float release, float sr);
            float process();
            void noteOn();
            void noteOff();
        } envelope;
        
        void prepare(double sampleRate) {
            resampler.prepare(sampleRate);
            antiAliasFilter.prepare(sampleRate);
            dcBlocker.reset();
            softClipper.prepare(sampleRate);
            envelope.sampleRate = static_cast<float>(sampleRate);
        }
        
        void reset() {
            isActive = false;
            position = 0.0;
            lastOutputSample = 0.0f;
            grains.clear();
            dcBlocker.reset();
        }
    };

    static constexpr int MAX_VOICES = 16;
    std::array<Voice, MAX_VOICES> voices;
    
    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReader> reader;
    juce::AudioBuffer<float> fileBuffer;
    juce::AudioBuffer<float> tempBuffer;
    
    double currentSampleRate = 44100.0;
    double fileSampleRate = 44100.0;
    double sampleRateRatio = 1.0;
    float playbackSpeed = 1.0f;
    bool isLooping = false;
    bool isHoldMode = false;
    double holdPosition = 0.0;
    bool isEnabled = true;
    PlaybackMode playbackMode = PlaybackMode::Polyphonic;  // Replace triggerMode
    std::atomic<float> currentLevel{0.0f};
    
    // Output processing
    DSPUtils::PeakLimiter outputLimiter;
    
    void startVoice(int midiNoteNumber, float velocity);
    void stopVoice(int midiNoteNumber);
    void updateGrains(Voice& voice);
    void applyFades();
    int findFreeVoice() const;
    void stealVoice();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SamplePlayer)
}; 