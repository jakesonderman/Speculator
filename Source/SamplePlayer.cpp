#include "SamplePlayer.h"
#include <cmath>

// Enhanced window function using raised cosine (Hann) with variable overlap
static float windowFunction(float phase, float overlap)
{
    // Extend the window edges for smoother overlap
    const float edge = (1.0f - overlap) * 0.5f;
    if (phase < edge)
        return 0.5f * (1.0f - std::cos(M_PI * phase / edge));
    else if (phase > (1.0f - edge))
        return 0.5f * (1.0f - std::cos(M_PI * (1.0f - phase) / edge));
    return 1.0f;
}

// Improved soft clipping function with smoother transition
static float softClip(float x)
{
    // Tanh-based soft clipper with more gradual knee
    return std::tanh(x * 0.8f);
}

// Anti-clicking function: apply short fade to prevent clicks
static float antiClick(float sample, float prevSample, float threshold = 0.3f)
{
    float diff = std::abs(sample - prevSample);
    if (diff > threshold) {
        // Apply smoothing when there's a large change
        return prevSample + (sample - prevSample) * 0.5f;
    }
    return sample;
}

SamplePlayer::SamplePlayer() : isEnabled(true), currentLevel(0.0f)
{
    formatManager.registerBasicFormats();
    
    // Initialize voices with slightly different parameters to prevent phase alignment
    for (auto& voice : voices)
    {
        voice.grainDuration = 0.1f;
        voice.grainOverlap = 0.5f;
    }
}

SamplePlayer::~SamplePlayer()
{
    releaseResources();
}

void SamplePlayer::loadFile(const juce::File& file)
{
    reader.reset(formatManager.createReaderFor(file));
    
    if (reader != nullptr)
    {
        fileBuffer.setSize(reader->numChannels, reader->lengthInSamples);
        reader->read(&fileBuffer, 0, reader->lengthInSamples, 0, true, true);
        
        fileSampleRate = reader->sampleRate;
        sampleRateRatio = currentSampleRate / fileSampleRate;
        
        // Normalize and apply DC blocking
        float maxSample = 0.0f;
        DSPUtils::DCBlocker dcBlocker;
        
        for (int channel = 0; channel < fileBuffer.getNumChannels(); ++channel)
        {
            float* channelData = fileBuffer.getWritePointer(channel);
            
            // First pass: find max sample and apply DC blocking
            for (int i = 0; i < fileBuffer.getNumSamples(); ++i)
            {
                channelData[i] = dcBlocker.process(channelData[i]);
                maxSample = std::max(maxSample, std::abs(channelData[i]));
            }
        }
        
        if (maxSample > 0.0f)
        {
            float scale = 0.95f / maxSample;
            fileBuffer.applyGain(scale);
        }
        
        applyFades();
    }
}

void SamplePlayer::applyFades()
{
    if (fileBuffer.getNumSamples() < 100)
        return;
        
    // Apply a short fade-in at the start
    int fadeLength = std::min(1000, fileBuffer.getNumSamples() / 10);
    for (int i = 0; i < fadeLength; ++i)
    {
        float gain = static_cast<float>(i) / fadeLength;
        // Use a smoother fade curve (cubic)
        float smoothGain = gain * gain * (3.0f - 2.0f * gain);
        
        for (int channel = 0; channel < fileBuffer.getNumChannels(); ++channel)
        {
            float* data = fileBuffer.getWritePointer(channel);
            data[i] *= smoothGain;
            
            // Also apply fade-out at the end
            data[fileBuffer.getNumSamples() - 1 - i] *= smoothGain;
        }
    }
}

void SamplePlayer::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    sampleRateRatio = currentSampleRate / fileSampleRate;
    
    tempBuffer.setSize(2, samplesPerBlock);
    
    // Initialize all DSP components
    outputLimiter.prepare(sampleRate);
    
    for (auto& voice : voices)
    {
        voice.prepare(sampleRate);
        voice.envelope.setParameters(0.01f, 0.1f, 0.7f, 0.2f, static_cast<float>(sampleRate));
    }
}

void SamplePlayer::releaseResources()
{
    reader.reset();
    fileBuffer.clear();
    for (auto& voice : voices)
    {
        voice.isActive = false;
        voice.grains.clear();
        voice.lastOutputSample = 0.0f;
    }
}

void SamplePlayer::processBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    if (!fileBuffer.getNumSamples() || !isEnabled)
        return;
        
    buffer.clear(startSample, numSamples);
    tempBuffer.clear();
    
    float maxLevel = 0.0f;
    
    // Process each voice
    for (auto& voice : voices)
    {
        if (!voice.isActive)
            continue;
            
        for (int sample = 0; sample < numSamples; ++sample)
        {
            float sampleValue = 0.0f;
            
            // Process grains
            for (auto& grain : voice.grains)
            {
                if (!grain.isActive)
                    continue;
                    
                // Calculate window position and gain
                grain.phase = static_cast<float>(grain.age / grain.grainLength);
                float windowGain = DSPUtils::GrainWindow::getGainAt(grain.phase, voice.grainOverlap);
                
                // Get interpolated sample with improved resampling
                float interpolatedSample = voice.resampler.resample(
                    fileBuffer.getReadPointer(0),
                    grain.currentPosition,
                    fileBuffer.getNumSamples()
                );
                
                // Apply phase alignment
                float phaseAlignedSample = interpolatedSample * 
                    std::cos(grain.initialPhase + grain.phaseIncrement * grain.age);
                
                sampleValue += phaseAlignedSample * windowGain;
                
                // Update grain position and age
                grain.currentPosition += voice.pitchRatio * playbackSpeed;
                grain.age++;
                
                if (grain.age >= grain.grainLength)
                    grain.isActive = false;
            }
            
            // Apply voice processing chain
            if (voice.pitchRatio > 1.0) {
                float cutoff = std::min(20000.0f, static_cast<float>(20000.0 / voice.pitchRatio));
                voice.antiAliasFilter.setCutoff(cutoff);
                sampleValue = voice.antiAliasFilter.process(sampleValue);
            }
            
            sampleValue = voice.dcBlocker.process(sampleValue);
            sampleValue = voice.softClipper.process(sampleValue);
            
            // Apply envelope
            float envelopeGain = voice.envelope.process();
            sampleValue *= envelopeGain * voice.velocity;
            
            // Add to temp buffer
            for (int channel = 0; channel < tempBuffer.getNumChannels(); ++channel)
                tempBuffer.addSample(channel, sample, sampleValue);
            
            maxLevel = std::max(maxLevel, std::abs(sampleValue));
            
            // Update grains
            updateGrains(voice);
        }
    }
    
    // Final output processing
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        float* outBuffer = buffer.getWritePointer(channel, startSample);
        const float* tempData = tempBuffer.getReadPointer(channel);
        
        for (int sample = 0; sample < numSamples; ++sample)
        {
            outBuffer[sample] = outputLimiter.process(tempData[sample]);
        }
    }
    
    currentLevel.store(maxLevel);
}

void SamplePlayer::handleMidiMessage(const juce::MidiMessage& message)
{
    // Check if a sample is loaded
    if (fileBuffer.getNumSamples() == 0)
        return;

    if (message.isNoteOn())
    {
        switch (playbackMode)
        {
            case PlaybackMode::Polyphonic:
                startVoice(message.getNoteNumber(), message.getFloatVelocity());
                break;
                
            case PlaybackMode::Monophonic:
                // Stop any existing voices before starting new one
                stopAllVoices();
                startVoice(message.getNoteNumber(), message.getFloatVelocity());
                break;
                
            case PlaybackMode::OneShot:
                // Start a new voice without stopping others
                startVoice(message.getNoteNumber(), message.getFloatVelocity());
                break;
        }
    }
    else if (message.isNoteOff())
    {
        // In Polyphonic mode, stop the specific note
        // In Monophonic and OneShot modes, notes continue playing
        if (playbackMode == PlaybackMode::Polyphonic)
        {
            stopVoice(message.getNoteNumber());
        }
    }
}

void SamplePlayer::startVoice(int midiNoteNumber, float velocity)
{
    // Find free voice or steal one if needed
    int voiceIndex = findFreeVoice();
    if (voiceIndex == -1)
    {
        // In OneShot mode, we need to be more careful about voice stealing
        if (playbackMode == PlaybackMode::OneShot)
        {
            // Try to find a voice that's finished its envelope
            for (size_t i = 0; i < voices.size(); ++i)
            {
                if (voices[i].envelope.state == Voice::Envelope::State::Idle)
                {
                    voiceIndex = static_cast<int>(i);
                    break;
                }
            }
        }
        
        // If still no voice found, steal one
        if (voiceIndex == -1)
        {
            stealVoice();
            voiceIndex = findFreeVoice();
        }
    }
    
    if (voiceIndex != -1)
    {
        auto& voice = voices[voiceIndex];
        voice.reset();
        
        voice.isActive = true;
        voice.midiNote = midiNoteNumber;
        voice.velocity = velocity;
        voice.position = isHoldMode ? holdPosition : 0.0;
        
        // Calculate pitch ratio from MIDI note
        const float noteRatio = std::pow(2.0f, (midiNoteNumber - 60) / 12.0f);
        voice.pitchRatio = noteRatio;
        
        // In OneShot and Monophonic modes, we don't use the envelope release
        if (playbackMode == PlaybackMode::OneShot || playbackMode == PlaybackMode::Monophonic)
        {
            voice.envelope.sustainLevel = 1.0f;  // Full sustain
            voice.envelope.releaseTime = 0.5f;   // Longer release for smoother stop
        }
        
        voice.envelope.noteOn();
    }
}

void SamplePlayer::stopVoice(int midiNoteNumber)
{
    // Only stop voices in Polyphonic mode
    if (playbackMode == PlaybackMode::Polyphonic)
    {
        for (auto& voice : voices)
        {
            if (voice.isActive && voice.midiNote == midiNoteNumber)
            {
                voice.envelope.noteOff();
            }
        }
    }
}

void SamplePlayer::updateGrains(Voice& voice)
{
    // Remove inactive grains
    voice.grains.erase(
        std::remove_if(voice.grains.begin(), voice.grains.end(),
            [](const Grain& g) { return !g.isActive; }),
        voice.grains.end()
    );
    
    // Create new grains as needed
    if (voice.grains.empty() || 
        voice.grains.back().phase >= voice.grainOverlap)
    {
        Grain newGrain;
        newGrain.startPosition = voice.position;
        newGrain.currentPosition = voice.position;
        newGrain.grainLength = voice.grainDuration * currentSampleRate;
        newGrain.isActive = true;
        
        // Calculate initial phase and phase increment for alignment
        newGrain.initialPhase = static_cast<float>(std::fmod(voice.position, 2.0 * M_PI));
        newGrain.phaseIncrement = static_cast<float>(2.0 * M_PI * voice.pitchRatio / newGrain.grainLength);
        
        voice.grains.push_back(newGrain);
    }
    
    // Update voice position
    if (!isHoldMode)
    {
        voice.position += voice.pitchRatio * playbackSpeed;
        
        // Handle looping
        if (isLooping && voice.position >= fileBuffer.getNumSamples())
        {
            voice.position = 0.0;
        }
        else if (!isLooping && voice.position >= fileBuffer.getNumSamples())
        {
            stopVoice(voice.midiNote);
        }
    }
}

int SamplePlayer::findFreeVoice() const
{
    for (size_t i = 0; i < voices.size(); ++i)
    {
        if (!voices[i].isActive)
            return static_cast<int>(i);
    }
    return -1;
}

void SamplePlayer::stealVoice()
{
    // Find the voice with the lowest volume
    int stealIndex = 0;
    float lowestLevel = std::numeric_limits<float>::max();
    
    for (size_t i = 0; i < voices.size(); ++i)
    {
        if (voices[i].envelope.currentLevel < lowestLevel)
        {
            lowestLevel = voices[i].envelope.currentLevel;
            stealIndex = static_cast<int>(i);
        }
    }
    
    voices[stealIndex].reset();
}

void SamplePlayer::setHoldMode(bool shouldHold)
{
    isHoldMode = shouldHold;
    if (shouldHold)
    {
        for (const auto& voice : voices)
        {
            if (voice.isActive)
            {
                holdPosition = voice.position;
                break;
            }
        }
    }
}

void SamplePlayer::setHoldPosition(double normalizedPosition)
{
    if (fileBuffer.getNumSamples() > 0)
    {
        holdPosition = normalizedPosition * fileBuffer.getNumSamples();
        if (isHoldMode)
        {
            for (auto& voice : voices)
            {
                if (voice.isActive)
                {
                    voice.position = holdPosition;
                }
            }
        }
    }
}

double SamplePlayer::getHoldPosition() const
{
    return fileBuffer.getNumSamples() > 0 ? 
        holdPosition / fileBuffer.getNumSamples() : 0.0;
}

double SamplePlayer::getCurrentPosition() const
{
    for (const auto& voice : voices)
    {
        if (voice.isActive)
        {
            return voice.position / 
                (fileBuffer.getNumSamples() > 0 ? fileBuffer.getNumSamples() : 1);
        }
    }
    return 0.0;
}

void SamplePlayer::stopAllVoices()
{
    for (auto& voice : voices)
    {
        if (voice.isActive)
        {
            // Use a quick release for immediate stop
            voice.envelope.releaseTime = 0.02f;
            voice.envelope.noteOff();
            voice.grains.clear();
            voice.reset();
        }
    }
    
    // Process one block to ensure release
    juce::AudioBuffer<float> tempBuffer(2, 512);
    tempBuffer.clear();
    processBlock(tempBuffer, 0, tempBuffer.getNumSamples());
}

void SamplePlayer::Voice::Envelope::setParameters(float attack, float decay, float sustain, float release, float sr)
{
    attackTime = attack;
    decayTime = decay;
    sustainLevel = sustain;
    releaseTime = release;
    sampleRate = sr;
}

float SamplePlayer::Voice::Envelope::process()
{
    float targetLevel = 0.0f;
    float timeInc = 1.0f / sampleRate;
    
    switch (state)
    {
        case State::Attack:
            currentTime += timeInc;
            currentLevel = currentTime / attackTime;
            if (currentLevel >= 1.0f)
            {
                currentLevel = 1.0f;
                state = State::Decay;
                currentTime = 0.0f;
            }
            break;
            
        case State::Decay:
            currentTime += timeInc;
            currentLevel = 1.0f - (1.0f - sustainLevel) * (currentTime / decayTime);
            if (currentLevel <= sustainLevel)
            {
                currentLevel = sustainLevel;
                state = State::Sustain;
            }
            break;
            
        case State::Sustain:
            currentLevel = sustainLevel;
            break;
            
        case State::Release:
            currentTime += timeInc;
            currentLevel = sustainLevel * (1.0f - currentTime / releaseTime);
            if (currentLevel <= 0.001f)
            {
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

void SamplePlayer::Voice::Envelope::noteOn()
{
    state = State::Attack;
    currentTime = 0.0f;
    if (state == State::Release)
    {
        currentTime = (currentLevel / 1.0f) * attackTime;
    }
}

void SamplePlayer::Voice::Envelope::noteOff()
{
    if (state != State::Idle)
    {
        state = State::Release;
        currentTime = 0.0f;
    }
}

void SamplePlayer::setPlaybackSpeed(float speed)
{
    playbackSpeed = speed;
}

void SamplePlayer::setLooping(bool shouldLoop)
{
    isLooping = shouldLoop;
}

double SamplePlayer::getLengthInSeconds() const
{
    return fileBuffer.getNumSamples() > 0 ? fileBuffer.getNumSamples() / fileSampleRate : 0.0;
} 