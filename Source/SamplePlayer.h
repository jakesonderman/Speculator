#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <vector>

class SamplePlayer
{
public:
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
    void setHoldMode(bool shouldHold) { isHoldMode = shouldHold; }
    bool getHoldMode() const { return isHoldMode; }
    bool isFileLoaded() const { return fileBuffer.getNumSamples() > 0; }
    double getLengthInSeconds() const;
    float getCurrentLevel() const { return currentLevel.load(); }
    double getCurrentPosition() const 
    { 
        // Return position of first active voice, or 0 if none active
        for (const auto& voice : voices)
            if (voice.isActive)
                return voice.position / (fileBuffer.getNumSamples() > 0 ? fileBuffer.getNumSamples() : 1);
        return 0.0;
    }
    
    // Enable/disable processing
    void setEnabled(bool enabled) { isEnabled = enabled; }
    bool getEnabled() const { return isEnabled; }

    void stopAllVoices()
    {
        // Trigger release phase for all active voices
        for (auto& voice : voices)
        {
            if (voice.isActive)
            {
                voice.envelope.noteOff();
            }
        }
    }

private:
    // Simple one-pole low-pass filter for anti-aliasing
    class OnePoleFilter
    {
    public:
        void initialize(double sampleRate);
        void setCutoff(float cutoffFreq);
        float process(float input);
        void reset();
        
    private:
        double sampleRate = 44100.0;
        float a = 0.9f;          // Filter coefficient
        float lastOutput = 0.0f; // Last output sample
    };

    // Envelope stages
    enum class EnvelopeStage
    {
        Attack,
        Decay,
        Sustain,
        Release,
        Idle
    };

    struct Envelope
    {
        EnvelopeStage stage = EnvelopeStage::Idle;
        float level = 0.0f;
        float attackRate;
        float decayRate;
        float sustainLevel;
        float releaseRate;
        
        void setParameters(float attackTime, float decayTime, float sustainLvl, float releaseTime, double sampleRate)
        {
            attackRate = 1.0f / (attackTime * sampleRate);
            decayRate = (1.0f - sustainLvl) / (decayTime * sampleRate);
            sustainLevel = sustainLvl;
            releaseRate = sustainLevel / (releaseTime * sampleRate);
        }
        
        float process()
        {
            switch (stage)
            {
                case EnvelopeStage::Attack:
                    level += attackRate;
                    if (level >= 1.0f)
                    {
                        level = 1.0f;
                        stage = EnvelopeStage::Decay;
                    }
                    break;
                    
                case EnvelopeStage::Decay:
                    level -= decayRate;
                    if (level <= sustainLevel)
                    {
                        level = sustainLevel;
                        stage = EnvelopeStage::Sustain;
                    }
                    break;
                    
                case EnvelopeStage::Release:
                    level -= releaseRate;
                    if (level <= 0.0f)
                    {
                        level = 0.0f;
                        stage = EnvelopeStage::Idle;
                    }
                    break;
                    
                default:
                    break;
            }
            
            return level;
        }
        
        void noteOn()
        {
            stage = EnvelopeStage::Attack;
            if (level == 0.0f)
                level = 1e-4f; // Small non-zero value to prevent clicks
        }
        
        void noteOff()
        {
            stage = EnvelopeStage::Release;
        }
    };

    struct Grain
    {
        double startPosition = 0.0;    // Start position in the sample
        double currentPosition = 0.0;  // Current playback position
        double grainLength = 0.0;      // Length of the grain in samples
        double age = 0.0;              // Current age of the grain
        bool isActive = false;
        bool isFadingOut = false;      // Flag for crossfade handling
        
        float getWindowValue() const
        {
            // Enhanced window function (Hann window with smoother edges)
            const double ratio = age / grainLength;
            const double window = 0.5 * (1.0 - std::cos(2.0 * juce::MathConstants<double>::pi * ratio));
            // Add extra smoothing at edges
            const double edgeWidth = 0.15; // Wider edge for smoother transitions
            if (ratio < edgeWidth)
                return window * std::pow(ratio / edgeWidth, 2.0); // Quadratic fade-in
            else if (ratio > (1.0 - edgeWidth))
                return window * std::pow((1.0 - ratio) / edgeWidth, 2.0); // Quadratic fade-out
            return window;
        }
    };

    struct Voice
    {
        bool isActive = false;
        double position = 0.0;
        double pitchRatio = 1.0;
        float velocity = 0.0f;
        int midiNote = -1;
        float lastOutputSample = 0.0f; // For anti-click smoothing
        
        // Envelope parameters
        struct Envelope {
            float attackTime = 0.01f;    // seconds
            float decayTime = 0.1f;      // seconds
            float sustainLevel = 0.7f;    // 0 to 1
            float releaseTime = 0.2f;     // seconds
            
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
            float sampleRate = 44100.0f;
            
            void setParameters(float attack, float decay, float sustain, float release, float sr) {
                attackTime = attack;
                decayTime = decay;
                sustainLevel = sustain;
                releaseTime = release;
                sampleRate = sr;
            }
            
            void noteOn() {
                state = State::Attack;
                currentTime = 0.0f;
                if (state == State::Release) {
                    // Start from current level for smoother transition
                    currentTime = (currentLevel / 1.0f) * attackTime * sampleRate;
                }
            }
            
            void noteOff() {
                if (state != State::Idle) {
                    state = State::Release;
                    currentTime = 0.0f;
                }
            }
            
            float process() {
                float targetLevel = 0.0f;
                float timeInc = 1.0f / sampleRate;
                
                switch (state) {
                    case State::Attack:
                        currentTime += timeInc;
                        currentLevel = currentTime / (attackTime * sampleRate);
                        if (currentLevel >= 1.0f) {
                            currentLevel = 1.0f;
                            state = State::Decay;
                            currentTime = 0.0f;
                        }
                        break;
                        
                    case State::Decay:
                        currentTime += timeInc;
                        currentLevel = 1.0f - (1.0f - sustainLevel) * (currentTime / (decayTime * sampleRate));
                        if (currentLevel <= sustainLevel) {
                            currentLevel = sustainLevel;
                            state = State::Sustain;
                        }
                        break;
                        
                    case State::Sustain:
                        currentLevel = sustainLevel;
                        break;
                        
                    case State::Release:
                        currentTime += timeInc;
                        currentLevel = sustainLevel * (1.0f - currentTime / (releaseTime * sampleRate));
                        if (currentLevel <= 0.001f) {
                            currentLevel = 0.0f;
                            state = State::Idle;
                        }
                        break;
                        
                    case State::Idle:
                        currentLevel = 0.0f;
                        break;
                }
                
                return currentLevel;
            }
        };
        
        Envelope envelope;
        OnePoleFilter filter;  // Anti-aliasing filter for each voice
        
        // Grain parameters
        std::vector<Grain> grains;
        double grainDuration = 0.1;  // seconds
        double grainOverlap = 0.5;   // 50% overlap
        size_t maxGrains = 4;
        
        // Crossfade parameters
        float crossfadeLength = 512.0f;  // samples
        
        Voice() {
            grains.reserve(maxGrains);
        }
    };

    std::unique_ptr<juce::AudioFormatReader> reader;
    juce::AudioFormatManager formatManager;
    juce::AudioBuffer<float> fileBuffer;
    juce::AudioBuffer<float> tempBuffer;  // For intermediate processing
    std::array<Voice, 16> voices;         // Support up to 16 voices
    float playbackSpeed = 1.0f;
    bool isLooping = false;
    bool isHoldMode = false;              // New parameter for hold mode
    bool isEnabled = true;
    double currentSampleRate = 44100.0;  // Current system sample rate
    double fileSampleRate = 44100.0;     // Sample rate of loaded file
    double sampleRateRatio = 1.0;        // Ratio between file and system sample rates
    std::atomic<float> currentLevel{0.0f};

    void startVoice(int midiNoteNumber, float velocity);
    void stopVoice(int midiNoteNumber);
    void updateGrains(Voice& voice);
    void applyFades();  // Apply fades to the sample buffer
}; 