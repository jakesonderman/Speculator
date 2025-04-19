#include "PluginProcessor.h"
#include "PluginEditor.h"

SondyQ2AudioProcessorEditor::SondyQ2AudioProcessorEditor(SondyQ2AudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    // Set up resizing behavior
    constrainer = std::make_unique<juce::ComponentBoundsConstrainer>();
    constrainer->setMinimumWidth(400);
    constrainer->setMinimumHeight(300);
    constrainer->setMaximumWidth(1200);
    constrainer->setMaximumHeight(800);
    
    resizeCorner = std::make_unique<juce::ResizableCornerComponent>(this, constrainer.get());
    addAndMakeVisible(resizeCorner.get());
    
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
    
    modeButton = std::make_unique<juce::TextButton>("Mode: Poly");
    modeButton->addListener(this);
    addAndMakeVisible(modeButton.get());
    
    speedSlider = std::make_unique<juce::Slider>(juce::Slider::SliderStyle::LinearHorizontal, 
                                               juce::Slider::TextEntryBoxPosition::TextBoxRight);
    speedSlider->setRange(0.1, 4.0, 0.01);
    speedSlider->setValue(1.0);
    speedSlider->setTextValueSuffix("x");
    speedSlider->addListener(this);
    addAndMakeVisible(speedSlider.get());
    
    // Set up waveform display
    waveformDisplay = std::make_unique<WaveformDisplay>();
    waveformDisplay->onPositionClicked = [this](double position) {
        if (auto* samplePlayer = audioProcessor.getSamplePlayer())
        {
            if (samplePlayer->getHoldMode())
            {
                samplePlayer->setHoldPosition(position);
            }
        }
    };
    addAndMakeVisible(waveformDisplay.get());
    
    // Start timer to refresh UI
    startTimerHz(30);
    
    // Set initial size
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
    
    // Position resize corner
    const int cornerSize = 20;
    resizeCorner->setBounds(getWidth() - cornerSize, getHeight() - cornerSize, cornerSize, cornerSize);
    
    // Calculate proportional heights
    const float waveformHeightRatio = 0.5f;  // 50% of total height for waveform
    const float buttonHeight = 30;  // Fixed height for buttons
    const float spacing = 10;       // Fixed spacing
    
    // Set up waveform display with proportional height
    int waveformHeight = static_cast<int>(area.getHeight() * waveformHeightRatio);
    waveformDisplay->setBounds(area.removeFromTop(waveformHeight));
    
    // Leave some space between waveform and controls
    area.removeFromTop(spacing);
    
    // Create a horizontal area for buttons
    auto buttonArea = area.removeFromTop(buttonHeight);
    
    // Calculate button widths based on available space
    int availableWidth = buttonArea.getWidth();
    int buttonSpacing = spacing;
    int numButtons = 5;  // loadButton, loopButton, holdButton, stopButton, modeButton
    int buttonWidth = (availableWidth - (buttonSpacing * (numButtons - 1))) / numButtons;
    
    // Layout controls with proportional widths
    loadButton->setBounds(buttonArea.removeFromLeft(buttonWidth));
    buttonArea.removeFromLeft(buttonSpacing);
    
    loopButton->setBounds(buttonArea.removeFromLeft(buttonWidth));
    buttonArea.removeFromLeft(buttonSpacing);
    
    holdButton->setBounds(buttonArea.removeFromLeft(buttonWidth));
    buttonArea.removeFromLeft(buttonSpacing);
    
    stopButton->setBounds(buttonArea.removeFromLeft(buttonWidth));
    buttonArea.removeFromLeft(buttonSpacing);
    
    modeButton->setBounds(buttonArea.removeFromLeft(buttonWidth));
    
    // Leave space for slider
    area.removeFromTop(spacing);
    
    // Speed slider with proportional height
    speedSlider->setBounds(area.removeFromTop(buttonHeight));
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
        
        // Update the hold position immediately when enabling hold mode
        if (shouldHold && audioProcessor.getSamplePlayer() != nullptr)
        {
            double currentPos = audioProcessor.getSamplePlayer()->getCurrentPosition();
            audioProcessor.getSamplePlayer()->setHoldPosition(currentPos);
        }
    }
    else if (button == stopButton.get())
    {
        if (auto* samplePlayer = audioProcessor.getSamplePlayer())
        {
            samplePlayer->stopAllVoices();
        }
    }
    else if (button == modeButton.get())
    {
        audioProcessor.cyclePlaybackMode();
        updateModeButtonText();
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

void SondyQ2AudioProcessorEditor::updateModeButtonText()
{
    if (auto* samplePlayer = audioProcessor.getSamplePlayer())
    {
        juce::String modeText = "Mode: ";
        switch (samplePlayer->getPlaybackMode())
        {
            case SamplePlayer::PlaybackMode::Polyphonic:
                modeText += "Poly";
                break;
            case SamplePlayer::PlaybackMode::Monophonic:
                modeText += "Mono";
                break;
            case SamplePlayer::PlaybackMode::OneShot:
                modeText += "OneShot";
                break;
        }
        modeButton->setButtonText(modeText);
    }
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