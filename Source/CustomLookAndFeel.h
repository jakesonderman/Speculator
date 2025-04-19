#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class CustomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CustomLookAndFeel()
    {
        setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xFF000000));
        setColour(juce::Slider::thumbColourId, juce::Colour(0xFF00FF00));
        setColour(juce::Slider::trackColourId, juce::Colour(0xFF003300));
        setColour(juce::Slider::backgroundColourId, juce::Colour(0xFF000000));
        setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF000000));
        setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF00FF00));
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF000000));
        setColour(juce::ComboBox::textColourId, juce::Colour(0xFF00FF00));
        setColour(juce::ComboBox::arrowColourId, juce::Colour(0xFF00FF00));
        setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF000000));
        setColour(juce::TextEditor::textColourId, juce::Colour(0xFF00FF00));
        setColour(juce::TextEditor::highlightColourId, juce::Colour(0xFF003300));
        setColour(juce::TextEditor::highlightedTextColourId, juce::Colour(0xFF00FF00));
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                             bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        
        // Draw pixelated border
        g.setColour(juce::Colour(0xFF00FF00));
        float pixelSize = 2.0f;
        
        // Draw top border
        for (float x = bounds.getX(); x < bounds.getRight(); x += pixelSize)
        {
            g.fillRect(x, bounds.getY(), pixelSize, pixelSize);
        }
        
        // Draw bottom border
        for (float x = bounds.getX(); x < bounds.getRight(); x += pixelSize)
        {
            g.fillRect(x, bounds.getBottom() - pixelSize, pixelSize, pixelSize);
        }
        
        // Draw left border
        for (float y = bounds.getY(); y < bounds.getBottom(); y += pixelSize)
        {
            g.fillRect(bounds.getX(), y, pixelSize, pixelSize);
        }
        
        // Draw right border
        for (float y = bounds.getY(); y < bounds.getBottom(); y += pixelSize)
        {
            g.fillRect(bounds.getRight() - pixelSize, y, pixelSize, pixelSize);
        }
        
        // Fill background
        g.setColour(backgroundColour);
        g.fillRect(bounds.reduced(pixelSize));
        
        // Draw highlight effect
        if (shouldDrawButtonAsDown)
        {
            g.setColour(juce::Colour(0xFF00FF00).withAlpha(0.3f));
            g.fillRect(bounds.reduced(pixelSize));
        }
        else if (shouldDrawButtonAsHighlighted)
        {
            g.setColour(juce::Colour(0xFF00FF00).withAlpha(0.1f));
            g.fillRect(bounds.reduced(pixelSize));
        }
    }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                         float sliderPos, float minSliderPos, float maxSliderPos,
                         const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        auto bounds = slider.getLocalBounds().toFloat();
        float pixelSize = 2.0f;
        
        // Draw track background
        g.setColour(slider.findColour(juce::Slider::backgroundColourId));
        g.fillRect(bounds);
        
        // Draw track
        g.setColour(slider.findColour(juce::Slider::trackColourId));
        auto trackBounds = bounds.reduced(pixelSize);
        g.fillRect(trackBounds);
        
        // Draw thumb as a pixelated block
        auto thumbWidth = 12.0f;
        auto thumbHeight = 20.0f;
        auto thumbX = sliderPos - thumbWidth / 2;
        auto thumbY = bounds.getCentreY() - thumbHeight / 2;
        
        g.setColour(slider.findColour(juce::Slider::thumbColourId));
        for (float px = thumbX; px < thumbX + thumbWidth; px += pixelSize)
        {
            for (float py = thumbY; py < thumbY + thumbHeight; py += pixelSize)
            {
                g.fillRect(px, py, pixelSize, pixelSize);
            }
        }
    }

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                         bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        float pixelSize = 2.0f;
        
        // Draw checkbox border
        g.setColour(juce::Colour(0xFF00FF00));
        auto checkboxSize = bounds.getHeight();
        auto checkboxBounds = juce::Rectangle<float>(bounds.getX(), bounds.getY(), checkboxSize, checkboxSize);
        
        // Draw checkbox border
        for (float x = checkboxBounds.getX(); x < checkboxBounds.getRight(); x += pixelSize)
        {
            g.fillRect(x, checkboxBounds.getY(), pixelSize, pixelSize);
            g.fillRect(x, checkboxBounds.getBottom() - pixelSize, pixelSize, pixelSize);
        }
        for (float y = checkboxBounds.getY(); y < checkboxBounds.getBottom(); y += pixelSize)
        {
            g.fillRect(checkboxBounds.getX(), y, pixelSize, pixelSize);
            g.fillRect(checkboxBounds.getRight() - pixelSize, y, pixelSize, pixelSize);
        }
        
        // Draw checkmark if toggled
        if (button.getToggleState())
        {
            g.setColour(juce::Colour(0xFF00FF00));
            for (float x = checkboxBounds.getX() + pixelSize; x < checkboxBounds.getRight() - pixelSize; x += pixelSize)
            {
                for (float y = checkboxBounds.getY() + pixelSize; y < checkboxBounds.getBottom() - pixelSize; y += pixelSize)
                {
                    g.fillRect(x, y, pixelSize, pixelSize);
                }
            }
        }
        
        // Draw text
        g.setColour(button.findColour(juce::ToggleButton::textColourId));
        g.setFont(juce::Font("Monaco", 12.0f, juce::Font::plain));
        g.drawText(button.getButtonText(),
                  checkboxBounds.getRight() + 5.0f, bounds.getY(),
                  bounds.getWidth() - checkboxBounds.getWidth() - 5.0f, bounds.getHeight(),
                  juce::Justification::centredLeft, true);
    }

    void drawTextEditorOutline(juce::Graphics& g, int width, int height, juce::TextEditor& textEditor) override
    {
        auto bounds = juce::Rectangle<float>(0, 0, width, height).toFloat();
        float pixelSize = 2.0f;
        
        // Draw pixelated border
        g.setColour(juce::Colour(0xFF00FF00));
        for (float x = bounds.getX(); x < bounds.getRight(); x += pixelSize)
        {
            g.fillRect(x, bounds.getY(), pixelSize, pixelSize);
            g.fillRect(x, bounds.getBottom() - pixelSize, pixelSize, pixelSize);
        }
        for (float y = bounds.getY(); y < bounds.getBottom(); y += pixelSize)
        {
            g.fillRect(bounds.getX(), y, pixelSize, pixelSize);
            g.fillRect(bounds.getRight() - pixelSize, y, pixelSize, pixelSize);
        }
    }
}; 