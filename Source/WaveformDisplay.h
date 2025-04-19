#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>

class WaveformDisplay : public juce::Component,
                       public juce::FileDragAndDropTarget,
                       public juce::ChangeListener
{
public:
    WaveformDisplay() : thumbnail(512, formatManager, thumbnailCache),
                       currentPosition(-1.0),
                       currentLevel(0.0f)
    {
        formatManager.registerBasicFormats();
        thumbnail.addChangeListener(this);
    }

    ~WaveformDisplay() override
    {
        thumbnail.removeChangeListener(this);
    }

    void setFile(const juce::File& file)
    {
        if (file.existsAsFile())
        {
            thumbnail.setSource(new juce::FileInputSource(file));
        }
    }

    void setPlayheadPosition(double pos)
    {
        if (currentPosition != pos)
        {
            currentPosition = pos;
            repaint();
        }
    }

    void setCurrentLevel(float level)
    {
        if (std::abs(currentLevel - level) > 0.001f)
        {
            currentLevel = level;
            repaint();
        }
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFF000000));  // Black background

        // Draw grid lines
        g.setColour(juce::Colour(0xFF003300));  // Dark green
        for (int x = 0; x < getWidth(); x += 20)
            g.drawVerticalLine(x, 0, getHeight());
        for (int y = 0; y < getHeight(); y += 20)
            g.drawHorizontalLine(y, 0, getWidth());

        if (thumbnail.getNumChannels() == 0)
        {
            g.setColour(juce::Colour(0xFF00FF00));  // Bright green
            juce::FontOptions options;
            options = options.withHeight(16.0f).withStyle("plain");
            g.setFont(juce::Font(options));
            g.drawText("No Sample Loaded", getLocalBounds(), juce::Justification::centred);
            return;
        }

        // Draw the waveform
        g.setColour(juce::Colour(0xFF00FF00));  // Bright green
        thumbnail.drawChannel(g, getLocalBounds().reduced(2),
                            0.0, thumbnail.getTotalLength(),
                            0, 1.0f);

        // Draw playhead
        if (currentPosition >= 0.0)
        {
            int playheadX = static_cast<int>(currentPosition * getWidth());
            g.setColour(juce::Colour(0xFF00FF00));  // Bright green

            // Draw pixelated playhead
            for (int y = 0; y < getHeight(); y += 4)
            {
                g.fillRect(playheadX - 1, y, 3, 2);
            }
        }

        // Draw level meter
        int levelWidth = 3;
        int levelHeight = static_cast<int>(currentLevel * getHeight());
        g.setColour(juce::Colour(0xFF00FF00));
        g.fillRect(getWidth() - levelWidth, getHeight() - levelHeight, 
                  levelWidth, levelHeight);
    }

    void changeListenerCallback(juce::ChangeBroadcaster*) override
    {
        repaint();
    }

    void mouseDown(juce::MouseEvent const& e) override
    {
        if (thumbnail.getNumChannels() > 0)
        {
            double clickPosition = e.x / static_cast<double>(getWidth());
            clickPosition = std::max(0.0, std::min(1.0, clickPosition));
            if (onPositionClicked)
                onPositionClicked(clickPosition);
        }
    }

    void mouseDrag(juce::MouseEvent const& e) override
    {
        if (thumbnail.getNumChannels() > 0)
        {
            double dragPosition = e.x / static_cast<double>(getWidth());
            dragPosition = std::max(0.0, std::min(1.0, dragPosition));
            if (onPositionClicked)
                onPositionClicked(dragPosition);
        }
    }

    bool isInterestedInFileDrag(const juce::StringArray& files) override
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

    void filesDropped(const juce::StringArray& files, int x, int y) override
    {
        for (const auto& file : files)
        {
            if (isInterestedInFileDrag(juce::StringArray(file)))
            {
                setFile(juce::File(file));
                break;
            }
        }
    }

    std::function<void(double)> onPositionClicked;

private:
    juce::AudioFormatManager formatManager;
    juce::AudioThumbnailCache thumbnailCache{100};
    juce::AudioThumbnail thumbnail;
    double currentPosition;
    float currentLevel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformDisplay)
}; 