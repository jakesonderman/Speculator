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
    
    // Initialize all voices with slightly different parameters
    // to prevent phase alignment issues
    for (size_t i = 0; i < voices.size(); ++i) {
        auto& voice = voices[i];
        voice.grainDuration = 0.1 + (i * 0.001); // Slightly vary grain durations
        voice.grainOverlap = 0.5 + (i * 0.01);   // Slightly vary overlaps
        voice.lastOutputSample = 0.0f;           // Track last output for anti-clicking
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
        
        // Store the file's sample rate and update the ratio
        fileSampleRate = reader->sampleRate;
        sampleRateRatio = currentSampleRate / fileSampleRate;
        
        // Normalize the buffer to prevent clipping
        float maxSample = 0.0f;
        for (int channel = 0; channel < fileBuffer.getNumChannels(); ++channel)
        {
            const float* channelData = fileBuffer.getReadPointer(channel);
            for (int i = 0; i < fileBuffer.getNumSamples(); ++i)
            {
                maxSample = std::max(maxSample, std::abs(channelData[i]));
            }
        }
        
        if (maxSample > 0.0f)
        {
            float scale = 0.95f / maxSample; // Leave some headroom
            fileBuffer.applyGain(scale);
        }
        
        // Apply a short fade-in/fade-out to the buffer to prevent clicks
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
    
    // Pre-allocate memory for temporary processing
    tempBuffer.setSize(2, samplesPerBlock);
    
    for (auto& voice : voices)
    {
        voice.envelope.sampleRate = static_cast<float>(sampleRate);
        voice.envelope.setParameters(0.01f, 0.1f, 0.7f, 0.2f, static_cast<float>(sampleRate));
        
        // Initialize anti-aliasing filter
        voice.filter.initialize(sampleRate);
        voice.filter.setCutoff(20000.0f); // Start with high cutoff
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
        
    // Clear the output in the target range
    buffer.clear(startSample, numSamples);
    
    // Clear temporary buffer
    tempBuffer.clear();
    
    float maxLevel = 0.0f;
    
    for (auto& voice : voices)
    {
        if (!voice.isActive)
            continue;
            
        // Process grains
        for (int sample = 0; sample < numSamples; ++sample)
        {
            float sampleValue = 0.0f;
            
            // Process existing grains
            for (auto& grain : voice.grains)
            {
                if (!grain.isActive)
                    continue;
                    
                // Calculate window position (0 to 1)
                float windowPos = grain.age / (voice.grainDuration * currentSampleRate);
                
                // Apply enhanced window function (combination of Hanning and edge smoothing)
                float window = 0.5f * (1.0f - std::cos(2.0f * M_PI * windowPos));
                
                // Smooth start and end conditions (more gradual fade)
                if (windowPos < 0.15f)
                    window *= (windowPos / 0.15f) * (windowPos / 0.15f); // Quadratic fade-in
                else if (windowPos > 0.85f)
                    window *= ((1.0f - windowPos) / 0.15f) * ((1.0f - windowPos) / 0.15f); // Quadratic fade-out
                
                // Get the sample using cubic interpolation with improved boundary checks
                double pos = grain.currentPosition;
                int pos0 = static_cast<int>(pos);
                
                // Ensure all positions are within valid range
                int numSamples = fileBuffer.getNumSamples();
                
                // Safe modulo operation
                pos0 = ((pos0 % numSamples) + numSamples) % numSamples;
                int pos1 = (pos0 + 1) % numSamples;
                int pos2 = (pos0 + 2) % numSamples;
                int pos3 = (pos0 + 3) % numSamples;
                
                float t = static_cast<float>(pos - std::floor(pos));
                float t2 = t * t;
                float t3 = t2 * t;
                
                float c0 = -0.5f * t3 + t2 - 0.5f * t;
                float c1 = 1.5f * t3 - 2.5f * t2 + 1.0f;
                float c2 = -1.5f * t3 + 2.0f * t2 + 0.5f * t;
                float c3 = 0.5f * t3 - 0.5f * t2;
                
                float y0 = fileBuffer.getSample(0, pos0);
                float y1 = fileBuffer.getSample(0, pos1);
                float y2 = fileBuffer.getSample(0, pos2);
                float y3 = fileBuffer.getSample(0, pos3);
                
                float interpolatedSample = c0 * y0 + c1 * y1 + c2 * y2 + c3 * y3;
                
                // Apply anti-aliasing when pitch shifting up
                if (voice.pitchRatio > 1.0) {
                    // Adjust filter cutoff based on pitch ratio
                    float cutoff = std::min(20000.0f, static_cast<float>(20000.0 / voice.pitchRatio));
                    voice.filter.setCutoff(cutoff);
                    interpolatedSample = voice.filter.process(interpolatedSample);
                }
                
                sampleValue += interpolatedSample * window;
                
                // Update grain position and age
                grain.currentPosition += voice.pitchRatio * playbackSpeed;
                grain.age++;
                
                if (grain.age >= voice.grainDuration * currentSampleRate)
                    grain.isActive = false;
            }
            
            // Apply envelope
            float envelopeGain = voice.envelope.process();
            sampleValue *= envelopeGain * voice.velocity;
            
            // Apply soft clipping to prevent harsh distortion
            sampleValue = softClip(sampleValue);
            
            // Anti-click treatment - smooth out any large jumps in amplitude
            sampleValue = antiClick(sampleValue, voice.lastOutputSample);
            voice.lastOutputSample = sampleValue;
            
            // Add to temp buffer for this voice
            for (int channel = 0; channel < tempBuffer.getNumChannels(); ++channel)
                tempBuffer.addSample(channel, sample, sampleValue);
            
            // Track maximum level
            maxLevel = std::max(maxLevel, std::abs(sampleValue));
            
            // Update grains for next sample
            updateGrains(voice);
            
            // Check if voice is finished
            if (voice.envelope.state == Voice::Envelope::State::Idle)
                voice.isActive = false;
        }
    }
    
    // Apply soft limiting to the mixed signal to avoid clipping
    for (int channel = 0; channel < tempBuffer.getNumChannels(); ++channel) {
        float* channelData = tempBuffer.getWritePointer(channel);
        
        for (int i = 0; i < numSamples; ++i) {
            // Apply soft clipping to the final output
            channelData[i] = softClip(channelData[i]);
        }
    }
    
    // Copy from temp buffer to main buffer
    for (int channel = 0; channel < std::min(buffer.getNumChannels(), tempBuffer.getNumChannels()); ++channel) {
        buffer.addFrom(channel, startSample, tempBuffer, channel, 0, numSamples);
    }
    
    // Update current level
    currentLevel.store(maxLevel);
}

void SamplePlayer::handleMidiMessage(const juce::MidiMessage& message)
{
    // Check if a sample is loaded
    if (fileBuffer.getNumSamples() == 0)
        return;

    if (message.isNoteOn())
    {
        startVoice(message.getNoteNumber(), message.getFloatVelocity());
    }
    else if (message.isNoteOff())
    {
        stopVoice(message.getNoteNumber());
    }
}

void SamplePlayer::startVoice(int midiNoteNumber, float velocity)
{
    // Find a free voice or steal the oldest one
    Voice* voice = nullptr;
    for (auto& v : voices)
    {
        if (!v.isActive || v.envelope.state == Voice::Envelope::State::Idle)
        {
            voice = &v;
            break;
        }
    }
    
    if (!voice)
    {
        // Find the voice with the longest release time
        voice = &voices[0];
        float longestTime = 0.0f;
        for (auto& v : voices)
        {
            if (v.envelope.currentTime > longestTime)
            {
                longestTime = v.envelope.currentTime;
                voice = &v;
            }
        }
    }
    
    if (voice)
    {
        // If we're replacing a voice that's still making sound, 
        // crossfade to prevent clicks
        bool wasActive = voice->isActive && voice->envelope.currentLevel > 0.01f;
        
        // Reset the voice
        voice->isActive = true;
        voice->midiNote = midiNoteNumber;
        // Slightly randomize velocity to prevent phase alignment
        voice->velocity = velocity * (0.98f + 0.04f * (rand() / (float)RAND_MAX));
        voice->position = 0.0;
        
        // Calculate pitch ratio with more precision
        voice->pitchRatio = std::pow(2.0, (midiNoteNumber - 60) / 12.0);
        
        // Apply slight detuning for thicker sound
        double detune = 1.0 + (rand() / (RAND_MAX * 500.0)); // +/- 0.2% detune
        voice->pitchRatio *= detune;
        
        // Clear existing grains if not preserving them for crossfade
        if (!wasActive) {
            voice->grains.clear();
        } else {
            // Mark existing grains for fade-out
            for (auto& grain : voice->grains) {
                grain.isFadingOut = true;
            }
        }
        
        // Reset filter state
        voice->filter.reset();
        
        voice->envelope.noteOn();
    }
}

void SamplePlayer::stopVoice(int midiNoteNumber)
{
    for (auto& voice : voices)
    {
        if (voice.isActive && voice.midiNote == midiNoteNumber)
        {
            voice.envelope.noteOff();
        }
    }
}

void SamplePlayer::updateGrains(Voice& voice)
{
    // Remove completely inactive grains
    voice.grains.erase(
        std::remove_if(voice.grains.begin(), voice.grains.end(),
                      [](const Grain& g) { return !g.isActive; }),
        voice.grains.end());
    
    // Create new grain if needed
    bool shouldCreateNewGrain = voice.grains.empty() ||
        (voice.grains.back().age >= voice.grainDuration * currentSampleRate * (1.0 - voice.grainOverlap));
    
    if (shouldCreateNewGrain && voice.grains.size() < voice.maxGrains)
    {
        Grain newGrain;
        newGrain.isActive = true;
        newGrain.currentPosition = voice.position;
        newGrain.age = 0;
        newGrain.isFadingOut = false;
        voice.grains.push_back(newGrain);
    }
    
    // Update voice position with improved looping logic
    voice.position += voice.pitchRatio * playbackSpeed;
    if (voice.position >= fileBuffer.getNumSamples())
    {
        if (isLooping) {
            // Implement smoother loop with crossfade
            double overshoot = voice.position - fileBuffer.getNumSamples();
            voice.position = overshoot;
            
            // Add a grain at the start for seamless loop
            if (voice.grains.size() < voice.maxGrains) {
                Grain loopGrain;
                loopGrain.isActive = true;
                loopGrain.currentPosition = voice.position;
                loopGrain.age = 0;
                loopGrain.isFadingOut = false;
                voice.grains.push_back(loopGrain);
            }
        } else if (isHoldMode) {
            // Hold mode - stay at the end of the sample
            voice.position = fileBuffer.getNumSamples() - 1;
        } else {
            // Normal mode - deactivate the voice when it reaches the end
            voice.isActive = false;
            voice.envelope.noteOff();
            
            // Clear all grains to stop playback completely
            voice.grains.clear();
        }
    }
}

// One-pole low-pass filter implementation
void SamplePlayer::OnePoleFilter::initialize(double sampleRate) {
    this->sampleRate = sampleRate;
    setCutoff(20000.0f);
    lastOutput = 0.0f;
}

void SamplePlayer::OnePoleFilter::setCutoff(float cutoffFreq) {
    // Clamp cutoff frequency to reasonable range
    cutoffFreq = std::max(20.0f, std::min(20000.0f, cutoffFreq));
    
    // Calculate filter coefficient
    float x = std::exp(-2.0f * M_PI * cutoffFreq / static_cast<float>(sampleRate));
    a = x;
}

float SamplePlayer::OnePoleFilter::process(float input) {
    lastOutput = input * (1.0f - a) + lastOutput * a;
    return lastOutput;
}

void SamplePlayer::OnePoleFilter::reset() {
    lastOutput = 0.0f;
}

void SamplePlayer::setPlaybackSpeed(float speed)
{
    // Smooth transitions in playback speed
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