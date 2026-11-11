#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace moshdeck::theme
{
inline const juce::Colour bg        { 0xff101016 };
inline const juce::Colour panel     { 0xff181822 };
inline const juce::Colour panelHi   { 0xff22222f };
inline const juce::Colour outline   { 0xff2e2e40 };
inline const juce::Colour text      { 0xffe6e6f0 };
inline const juce::Colour dim       { 0xff8a8aa0 };
inline const juce::Colour accent    { 0xffff2d95 }; // hot pink
inline const juce::Colour accent2   { 0xff21e6ff }; // cyan
inline const juce::Colour mod       { 0xffffc021 }; // modulation indicator
inline const juce::Colour midi      { 0xff6dff5a }; // MIDI learn

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
        setColourScheme ({ bg, panel, panelHi, outline, text, accent, juce::Colours::white, panelHi, text });
        setColour (juce::Slider::thumbColourId, accent);
        setColour (juce::Slider::trackColourId, accent.withAlpha (0.7f));
        setColour (juce::Slider::backgroundColourId, outline);
        setColour (juce::Slider::rotarySliderFillColourId, accent);
        setColour (juce::Slider::rotarySliderOutlineColourId, outline);
        setColour (juce::TextButton::buttonColourId, panelHi);
        setColour (juce::TextButton::buttonOnColourId, accent);
        setColour (juce::ComboBox::backgroundColourId, panelHi);
        setColour (juce::ComboBox::outlineColourId, outline);
        setColour (juce::ListBox::backgroundColourId, panel);
        setColour (juce::TabbedButtonBar::tabOutlineColourId, outline);
        setColour (juce::TabbedComponent::backgroundColourId, panel);
        setColour (juce::ToggleButton::tickColourId, accent2);
        setColour (juce::Label::textColourId, text);
        setColour (juce::PopupMenu::backgroundColourId, panel);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.6f));
        setColour (juce::CodeEditorComponent::backgroundColourId, juce::Colour (0xff0c0c12));
        setColour (juce::CodeEditorComponent::defaultTextColourId, text);
        setColour (juce::CodeEditorComponent::lineNumberBackgroundId, panel);
        setColour (juce::ScrollBar::thumbColourId, outline.brighter (0.3f));
    }
};
} // namespace moshdeck::theme
