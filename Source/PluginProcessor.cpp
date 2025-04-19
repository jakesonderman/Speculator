#include "PluginProcessor.h"
#include "PluginEditor.h"

SondyQ2AudioProcessor::SondyQ2AudioProcessor()
    : AudioProcessor (BusesProperties()
                     .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                     .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    samplePlayer = std::make_unique<SamplePlayer>();
}

SondyQ2AudioProcessor::~SondyQ2AudioProcessor()
{
}

const juce::String SondyQ2AudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SondyQ2AudioProcessor::acceptsMidi() const
{
    return true;
}

bool SondyQ2AudioProcessor::producesMidi() const
{
    return false;
}

bool SondyQ2AudioProcessor::isMidiEffect() const
{
    return false;
}

double SondyQ2AudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SondyQ2AudioProcessor::getNumPrograms()
{
    return 1;
}

int SondyQ2AudioProcessor::getCurrentProgram()
{
    return 0;
}

void SondyQ2AudioProcessor::setCurrentProgram (int index)
{
}

const juce::String SondyQ2AudioProcessor::getProgramName (int index)
{
    return {};
}

void SondyQ2AudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

void SondyQ2AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    samplePlayer->prepareToPlay(sampleRate, samplesPerBlock);
}

void SondyQ2AudioProcessor::releaseResources()
{
    samplePlayer->releaseResources();
}

bool SondyQ2AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void SondyQ2AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any output channels that don't have input data
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Process MIDI messages and pass to SamplePlayer
    for (const auto metadata : midiMessages)
    {
        const auto message = metadata.getMessage();
        samplePlayer->handleMidiMessage(message);
    }
    
    // Process audio through SamplePlayer
    samplePlayer->processBlock(buffer, 0, buffer.getNumSamples());
    
    // Make sure all output channels are populated (in case SamplePlayer only filled the first channel)
    for (int channel = 1; channel < totalNumOutputChannels; ++channel)
    {
        const float* channelData = buffer.getReadPointer(0);
        float* outputData = buffer.getWritePointer(channel);
        
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            outputData[sample] = channelData[sample];
        }
    }
    
    // Update UI from audio thread
    auto* editor = dynamic_cast<SondyQ2AudioProcessorEditor*>(getActiveEditor());
    if (editor != nullptr)
    {
        editor->updateCurrentLevel(samplePlayer->getCurrentLevel());
        editor->updatePlayheadPosition(samplePlayer->getCurrentPosition());
    }
}

bool SondyQ2AudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* SondyQ2AudioProcessor::createEditor()
{
    return new SondyQ2AudioProcessorEditor (*this);
}

void SondyQ2AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
}

void SondyQ2AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
}

void SondyQ2AudioProcessor::loadSample(const juce::File& file)
{
    if (samplePlayer == nullptr)
        return;
        
    samplePlayer->loadFile(file);
}

void SondyQ2AudioProcessor::setPlaybackSpeed(float speed)
{
    samplePlayer->setPlaybackSpeed(speed);
}

void SondyQ2AudioProcessor::setLooping(bool shouldLoop)
{
    samplePlayer->setLooping(shouldLoop);
}

void SondyQ2AudioProcessor::setHoldMode(bool shouldHold)
{
    if (samplePlayer != nullptr)
        samplePlayer->setHoldMode(shouldHold);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SondyQ2AudioProcessor();
}