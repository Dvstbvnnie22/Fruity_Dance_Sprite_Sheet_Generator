#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "ParamSlider.h"

class MoshDeckProcessor;

namespace moshdeck
{
/** The big horizontal fader. Drag = move, double-click = centre, wheel = nudge, right-click = MIDI learn / modulate menu. */
class CrossfaderBar : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit CrossfaderBar (MoshDeckProcessor& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void setFromX (float x);
    MoshDeckProcessor& proc;
};

/** Crossfader strip: fader, mode (Fade / Cut), curve, reverse, cut point, smoothing and MIDI learn. */
class CrossfaderPanel : public juce::Component, private juce::Timer
{
public:
    explicit CrossfaderPanel (MoshDeckProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    MoshDeckProcessor& proc;
    CrossfaderBar bar;
    juce::Label title, modeL, curveL, cutL, smoothL, status;
    juce::ComboBox mode, curve;
    juce::TextButton rev { "REV" }, learn { "MIDI Learn" };
    juce::Slider cut, smooth;
};
} // namespace moshdeck
