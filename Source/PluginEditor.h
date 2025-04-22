#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "CustomLookAndFeel.h"
#include "WaveformDisplay.h"
#include "SamplePlayer.h"

//==============================================================================
class SondyQ2AudioProcessorEditor : public juce::AudioProcessorEditor,
                                  private juce::Button::Listener,
                                  private juce::Slider::Listener,
                                  private juce::Timer,
                                  private juce::FileDragAndDropTarget
{
public:
    SondyQ2AudioProcessorEditor(SondyQ2AudioProcessor&);
    ~SondyQ2AudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    void buttonClicked(juce::Button* button) override;
    void sliderValueChanged(juce::Slider* slider) override;
    void timerCallback() override;

    void updatePlayheadPosition(double position);
    void updateCurrentLevel(float level);

private:
    SondyQ2AudioProcessor& audioProcessor;
    std::unique_ptr<WaveformDisplay> waveformDisplay;
    std::unique_ptr<juce::TextButton> loadButton;
    std::unique_ptr<juce::TextButton> loopButton;
    std::unique_ptr<juce::TextButton> holdButton;
    std::unique_ptr<juce::TextButton> stopButton;
    std::unique_ptr<juce::TextButton> modeButton;
    std::unique_ptr<juce::Slider> speedSlider;
    std::unique_ptr<juce::Slider> grainSizeSlider;
    CustomLookAndFeel customLookAndFeel;
    
    std::unique_ptr<juce::FileChooser> fileChooser;
    
    // Resizing components
    std::unique_ptr<juce::ResizableCornerComponent> resizeCorner;
    std::unique_ptr<juce::ComponentBoundsConstrainer> constrainer;

    void loadFile(const juce::File& file);
    void updateThumbnail();

    void loadButtonClicked();
    void speedSliderValueChanged();
    void loopButtonClicked();
    void updateLoopButtonText();
    void updateHoldButtonText();
    void updateModeButtonText();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SondyQ2AudioProcessorEditor)
};