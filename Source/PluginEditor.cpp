#include "PluginProcessor.h"
#include "PluginEditor.h"

SondyQ2AudioProcessorEditor::SondyQ2AudioProcessorEditor(SondyQ2AudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    // Apply custom look and feel
    setLookAndFeel(&customLookAndFeel);
    
    // Initialize controls
    loadButton = std::make_unique<juce::TextButton>("Load Sample");
    loadButton->addListener(this);
    addAndMakeVisible(loadButton.get());
    
    loopButton = std::make_unique<juce::TextButton>("Loop: OFF");
    loopButton->setClickingTogglesState(true);
    loopButton->addListener(this);
    addAndMakeVisible(loopButton.get());
    
    holdButton = std::make_unique<juce::TextButton>("Hold: OFF");
    holdButton->setClickingTogglesState(true);
    holdButton->addListener(this);
    addAndMakeVisible(holdButton.get());
    
    stopButton = std::make_unique<juce::TextButton>("Stop");
    stopButton->addListener(this);
    addAndMakeVisible(stopButton.get());
    
    speedSlider = std::make_unique<juce::Slider>(juce::Slider::SliderStyle::LinearHorizontal, 
                                               juce::Slider::TextEntryBoxPosition::TextBoxRight);
    speedSlider->setRange(0.1, 4.0, 0.01);
    speedSlider->setValue(1.0);
    speedSlider->setTextValueSuffix("x");
    speedSlider->addListener(this);
    addAndMakeVisible(speedSlider.get());
    
    // Set up waveform display
    waveformDisplay = std::make_unique<WaveformDisplay>();
    addAndMakeVisible(waveformDisplay.get());
    
    // Start timer to refresh UI
    startTimerHz(30);
    
    setSize(600, 400);
}

SondyQ2AudioProcessorEditor::~SondyQ2AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void SondyQ2AudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF000000));  // Black background
    
    // Draw a grid of green lines
    g.setColour(juce::Colour(0xFF002200));  // Dark green
    
    // Draw vertical lines
    for (int x = 0; x < getWidth(); x += 20)
        g.drawVerticalLine(x, 0, getHeight());
    
    // Draw horizontal lines
    for (int y = 0; y < getHeight(); y += 20)
        g.drawHorizontalLine(y, 0, getWidth());
}

void SondyQ2AudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(10);
    
    // Set up waveform display
    waveformDisplay->setBounds(area.removeFromTop(200));
    
    // Leave some space between waveform and controls
    area.removeFromTop(10);
    
    // Create a horizontal area for buttons
    auto buttonArea = area.removeFromTop(40);
    
    // Layout controls
    loadButton->setBounds(buttonArea.removeFromLeft(100));
    
    buttonArea.removeFromLeft(10); // Space between buttons
    loopButton->setBounds(buttonArea.removeFromLeft(100));
    
    buttonArea.removeFromLeft(10); // Space between buttons
    holdButton->setBounds(buttonArea.removeFromLeft(100));
    
    buttonArea.removeFromLeft(10); // Space between buttons
    stopButton->setBounds(buttonArea.removeFromLeft(100));
    
    // Leave space for slider
    area.removeFromTop(10);
    
    // Speed slider
    speedSlider->setBounds(area.removeFromTop(40));
}

void SondyQ2AudioProcessorEditor::buttonClicked(juce::Button* button)
{
    if (button == loadButton.get())
    {
        loadButtonClicked();
    }
    else if (button == loopButton.get())
    {
        bool shouldLoop = loopButton->getToggleState();
        audioProcessor.setLooping(shouldLoop);
        updateLoopButtonText();
    }
    else if (button == holdButton.get())
    {
        bool shouldHold = holdButton->getToggleState();
        audioProcessor.setHoldMode(shouldHold);
        updateHoldButtonText();
    }
    else if (button == stopButton.get())
    {
        if (auto* samplePlayer = audioProcessor.getSamplePlayer())
        {
            samplePlayer->stopAllVoices();
        }
    }
}

void SondyQ2AudioProcessorEditor::sliderValueChanged(juce::Slider* slider)
{
    if (slider == speedSlider.get())
    {
        float speed = static_cast<float>(speedSlider->getValue());
        audioProcessor.setPlaybackSpeed(speed);
    }
}

void SondyQ2AudioProcessorEditor::timerCallback()
{
    if (auto* samplePlayer = audioProcessor.getSamplePlayer())
    {
        updateCurrentLevel(samplePlayer->getCurrentLevel());
        updatePlayheadPosition(samplePlayer->getCurrentPosition());
    }
}

bool SondyQ2AudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& file : files)
    {
        if (file.endsWithIgnoreCase(".wav") || 
            file.endsWithIgnoreCase(".aif") ||
            file.endsWithIgnoreCase(".aiff") ||
            file.endsWithIgnoreCase(".mp3"))
            return true;
    }
    return false;
}

void SondyQ2AudioProcessorEditor::loadButtonClicked()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Select a sample to load...",
        juce::File{},
        "*.wav;*.aif;*.aiff;*.mp3"
    );
    
    auto flags = juce::FileBrowserComponent::openMode 
                | juce::FileBrowserComponent::canSelectFiles;
                
    fileChooser->launchAsync(flags, [this](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file != juce::File{})
        {
            loadFile(file);
        }
    });
}

void SondyQ2AudioProcessorEditor::loadFile(const juce::File& file)
{
    audioProcessor.loadSample(file);
    if (waveformDisplay != nullptr)
    {
        waveformDisplay->setFile(file);
    }
}

void SondyQ2AudioProcessorEditor::filesDropped(const juce::StringArray& files, int x, int y)
{
    for (const auto& file : files)
    {
        if (isInterestedInFileDrag(juce::StringArray(file)))
        {
            loadFile(juce::File(file));
            break;
        }
    }
}

void SondyQ2AudioProcessorEditor::updateLoopButtonText()
{
    bool isLooping = loopButton->getToggleState();
    loopButton->setButtonText(isLooping ? "Loop: ON" : "Loop: OFF");
}

void SondyQ2AudioProcessorEditor::updateHoldButtonText()
{
    bool isHoldMode = holdButton->getToggleState();
    holdButton->setButtonText(isHoldMode ? "Hold: ON" : "Hold: OFF");
}

void SondyQ2AudioProcessorEditor::updatePlayheadPosition(double position)
{
    if (waveformDisplay != nullptr)
    {
        waveformDisplay->setPlayheadPosition(position);
    }
}

void SondyQ2AudioProcessorEditor::updateCurrentLevel(float level)
{
    if (waveformDisplay != nullptr)
        waveformDisplay->setCurrentLevel(level);
} 