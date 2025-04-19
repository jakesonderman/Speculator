#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>

class WaveformDisplay : public juce::Component,
                        private juce::Timer
{
public:
    WaveformDisplay() : currentLevel(0.0f), playheadPosition(0.0)
    {
        thumbnail = std::make_unique<juce::AudioThumbnail>(512, formatManager, thumbnailCache);
        formatManager.registerBasicFormats();
        startTimerHz(30);
    }
    
    ~WaveformDisplay() override
    {
        stopTimer();
    }
    
    void setFile(const juce::File& file)
    {
        if (file.existsAsFile())
        {
            thumbnail->setSource(new juce::FileInputSource(file));
            currentFile = file;
            repaint();
        }
    }
    
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFF000000)); // Black background
        
        // Draw grid lines
        g.setColour(juce::Colour(0xFF003300)); // Dark green
        
        // Vertical lines
        for (int x = 0; x < getWidth(); x += 20)
            g.drawVerticalLine(x, 0, getHeight());
        
        // Horizontal lines
        for (int y = 0; y < getHeight(); y += 20)
            g.drawHorizontalLine(y, 0, getWidth());
        
        // Draw waveform if available
        if (thumbnail->getTotalLength() > 0.0)
        {
            g.setColour(juce::Colour(0xFF00FF00)); // Bright green
            thumbnail->drawChannels(g, getLocalBounds(), 0.0, thumbnail->getTotalLength(), 1.0f);
            
            // Draw playhead as dashed line
            g.setColour(juce::Colours::yellow);
            int playheadX = static_cast<int>(playheadPosition * getWidth());
            
            // Draw pixelated playhead line
            for (int y = 0; y < getHeight(); y += 4)
            {
                g.fillRect(playheadX, y, 2, 2);
            }
            
            // Draw level meter
            int levelHeight = static_cast<int>(currentLevel * getHeight());
            g.setColour(juce::Colour(0xFF33FF33));
            g.fillRect(getWidth() - 10, getHeight() - levelHeight, 8, levelHeight);
        }
        else
        {
            // No sample loaded
            g.setColour(juce::Colour(0xFF00FF00)); // Bright green
            g.setFont(juce::Font("Monaco", 16.0f, juce::Font::plain));
            g.drawText("No Sample Loaded", getLocalBounds(), juce::Justification::centred);
        }
    }
    
    void setCurrentLevel(float level)
    {
        currentLevel = level;
    }
    
    void setPlayheadPosition(double position)
    {
        playheadPosition = position;
    }
    
    void timerCallback() override
    {
        repaint();
    }
    
private:
    juce::AudioFormatManager formatManager;
    juce::AudioThumbnailCache thumbnailCache{100};
    std::unique_ptr<juce::AudioThumbnail> thumbnail;
    juce::File currentFile;
    float currentLevel;
    double playheadPosition;
}; 