#include "CrossfaderPanel.h"
#include "Theme.h"
#include "PluginProcessor.h"
#include "model/Crossfader.h"

namespace moshdeck
{
//==============================================================================
CrossfaderBar::CrossfaderBar (MoshDeckProcessor& p) : proc (p)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTooltip ("Crossfader. Drag to move, double-click to centre, mouse wheel to nudge, right-click for MIDI learn / modulation.");
}

void CrossfaderBar::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    auto track = area.reduced (30.0f, 0.0f).removeFromTop (area.getHeight() - 10.0f).reduced (0.0f, 3.0f);
    auto outStrip = area.reduced (30.0f, 0.0f).removeFromBottom (5.0f);

    const int mode = (int) std::round (proc.scene.getValue ("G.xfade.mode"));
    const int curve = (int) std::round (proc.scene.getValue ("G.xfade.curve"));
    const bool rev = proc.scene.getValue ("G.xfade.rev") > 0.5f;
    const float pos = proc.scene.getValue (Scene::crossfaderId);
    const float cut = proc.scene.getValue ("G.xfade.cut");
    const float shaped = xf::shape (pos, mode, curve, rev, cut);

    // labels: A on the left, B on the right (swapped while reversed)
    g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    g.setColour (theme::accent2);
    g.drawText (rev ? "B" : "A", getLocalBounds().removeFromLeft (30), juce::Justification::centred);
    g.setColour (theme::accent);
    g.drawText (rev ? "A" : "B", getLocalBounds().removeFromRight (30), juce::Justification::centred);

    // track with an A -> B colour ramp
    g.setColour (theme::outline);
    g.fillRoundedRectangle (track, 6.0f);
    juce::ColourGradient ramp (theme::accent2.withAlpha (0.55f), track.getX(), track.getY(),
                               theme::accent.withAlpha (0.55f), track.getRight(), track.getY(), false);
    g.setGradientFill (ramp);
    g.fillRoundedRectangle (track.reduced (1.0f), 5.0f);

    // cut point marker (only meaningful in Cut mode)
    if (mode == xf::modeCut)
    {
        const float c = juce::jlimit (0.0f, 1.0f, cut);
        const float x = track.getX() + (rev ? 1.0f - c : c) * track.getWidth();
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.fillRect (juce::Rectangle<float> (x - 1.0f, track.getY() - 3.0f, 2.0f, track.getHeight() + 6.0f));
    }

    // thumb = where the fader is
    const float thumbX = track.getX() + pos * track.getWidth();
    g.setColour (juce::Colours::white);
    g.fillRoundedRectangle (juce::Rectangle<float> (thumbX - 7.0f, track.getY() - 4.0f, 14.0f, track.getHeight() + 8.0f), 3.0f);

    // output strip = how much of deck B the video actually gets after the curve / cut
    g.setColour (theme::mod);
    g.fillRect (outStrip.withWidth (outStrip.getWidth() * shaped));
}

void CrossfaderBar::setFromX (float x)
{
    const auto t = getLocalBounds().toFloat().reduced (30.0f, 0.0f);
    proc.scene.setNormalised (Scene::crossfaderId, (x - t.getX()) / juce::jmax (1.0f, t.getWidth()));
}

void CrossfaderBar::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        ParamSlider::showMenu (proc, Scene::crossfaderId, this);
        return;
    }
    setFromX (e.position.x);
}

void CrossfaderBar::mouseDrag (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu())
        setFromX (e.position.x);
}

void CrossfaderBar::mouseDoubleClick (const juce::MouseEvent&)
{
    proc.scene.setNormalised (Scene::crossfaderId, 0.5f);
}

void CrossfaderBar::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    proc.scene.setNormalised (Scene::crossfaderId, proc.scene.getNormalised (Scene::crossfaderId) + w.deltaY * 0.1f);
}

//==============================================================================
CrossfaderPanel::CrossfaderPanel (MoshDeckProcessor& p) : proc (p), bar (p)
{
    title.setText ("CROSSFADER", juce::dontSendNotification);
    title.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, theme::accent);
    addAndMakeVisible (title);
    status.setFont (juce::FontOptions (11.0f));
    status.setColour (juce::Label::textColourId, theme::dim);
    addAndMakeVisible (status);

    auto label = [this] (juce::Label& l, const char* text)
    {
        l.setText (text, juce::dontSendNotification);
        l.setFont (juce::FontOptions (11.0f));
        l.setColour (juce::Label::textColourId, theme::dim);
        addAndMakeVisible (l);
    };
    label (modeL, "Mode");
    label (curveL, "Curve");
    label (cutL, "Cut at");
    label (smoothL, "Smooth ms");

    mode.addItem ("Fade", 1);
    mode.addItem ("Cut", 2);
    mode.onChange = [this] { proc.scene.setValue ("G.xfade.mode", (float) (mode.getSelectedId() - 1)); };
    curve.addItem ("Linear", 1);
    curve.addItem ("Smooth", 2);
    curve.addItem ("Sharp A (late B)", 3);
    curve.addItem ("Sharp B (early B)", 4);
    curve.onChange = [this] { proc.scene.setValue ("G.xfade.curve", (float) (curve.getSelectedId() - 1)); };
    addAndMakeVisible (mode);
    addAndMakeVisible (curve);

    rev.setClickingTogglesState (true);
    rev.setTooltip ("Reverse the crossfader direction (hamster / inverse switch)");
    rev.onClick = [this] { proc.scene.setValue ("G.xfade.rev", rev.getToggleState() ? 1.0f : 0.0f); };
    addAndMakeVisible (rev);

    learn.setTooltip ("Click, then move a MIDI knob, fader or button to map the crossfader");
    learn.onClick = [this] { proc.midiLearn.arm (Scene::crossfaderId); };
    addAndMakeVisible (learn);

    cut.setSliderStyle (juce::Slider::LinearHorizontal);
    cut.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    cut.setRange (0.0, 1.0, 0.01);
    cut.setTooltip ("Where the hard cut happens (Cut mode)");
    cut.onValueChange = [this] { proc.scene.setValue ("G.xfade.cut", (float) cut.getValue()); };
    addAndMakeVisible (cut);

    smooth.setSliderStyle (juce::Slider::LinearHorizontal);
    smooth.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    smooth.setRange (0.0, 500.0, 1.0);
    smooth.setTooltip ("Softens the cut: 0 = instant, higher = the video glides over this many milliseconds");
    smooth.onValueChange = [this] { proc.scene.setValue ("G.xfade.smooth", (float) smooth.getValue()); };
    addAndMakeVisible (smooth);

    addAndMakeVisible (bar);
    startTimerHz (30);
    timerCallback();
}

void CrossfaderPanel::paint (juce::Graphics& g)
{
    g.fillAll (theme::panel);
    g.setColour (theme::outline);
    g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
}

void CrossfaderPanel::resized()
{
    auto r = getLocalBounds().reduced (8, 4);
    auto left = r.removeFromLeft (120);
    title.setBounds (left.removeFromTop (18));
    status.setBounds (left);

    auto right = r.removeFromRight (470);
    auto row1 = right.removeFromTop (22);
    right.removeFromTop (4);
    auto row2 = right.removeFromTop (22);

    modeL.setBounds (row1.removeFromLeft (38));
    mode.setBounds (row1.removeFromLeft (84).reduced (1, 0));
    row1.removeFromLeft (6);
    curveL.setBounds (row1.removeFromLeft (40));
    curve.setBounds (row1.removeFromLeft (140).reduced (1, 0));
    row1.removeFromLeft (6);
    rev.setBounds (row1.removeFromLeft (56).reduced (1, 0));
    row1.removeFromLeft (6);
    learn.setBounds (row1.removeFromLeft (100).reduced (1, 0));

    cutL.setBounds (row2.removeFromLeft (52));
    cut.setBounds (row2.removeFromLeft (180));
    row2.removeFromLeft (6);
    smoothL.setBounds (row2.removeFromLeft (70));
    smooth.setBounds (row2.removeFromLeft (120));

    bar.setBounds (r.reduced (4, 0));
}

void CrossfaderPanel::timerCallback()
{
    const int m = juce::jlimit (0, 1, (int) std::round (proc.scene.getValue ("G.xfade.mode")));
    const int c = juce::jlimit (0, xf::numCurves - 1, (int) std::round (proc.scene.getValue ("G.xfade.curve")));
    const bool r = proc.scene.getValue ("G.xfade.rev") > 0.5f;
    const float cv = proc.scene.getValue ("G.xfade.cut");
    const float sv = proc.scene.getValue ("G.xfade.smooth");
    const float pos = proc.scene.getValue (Scene::crossfaderId);

    mode.setSelectedId (m + 1, juce::dontSendNotification);
    curve.setSelectedId (c + 1, juce::dontSendNotification);
    rev.setToggleState (r, juce::dontSendNotification);
    if (std::abs (cut.getValue() - cv) > 0.001) cut.setValue (cv, juce::dontSendNotification);
    if (std::abs (smooth.getValue() - sv) > 0.5) smooth.setValue (sv, juce::dontSendNotification);

    const float shaped = xf::shape (pos, m, c, r, cv);
    status.setText (juce::String (juce::roundToInt ((1.0f - shaped) * 100.0f)) + "% A | "
                    + juce::String (juce::roundToInt (shaped * 100.0f)) + "% B  ("
                    + xf::modeName (m) + ", " + xf::curveName (c) + (r ? ", reversed" : "") + ")",
                    juce::dontSendNotification);

    const bool armed = proc.midiLearn.getArmed() == Scene::crossfaderId;
    const auto mapped = proc.midiLearn.describeMapping (Scene::crossfaderId);
    learn.setButtonText (armed ? "Move a control..." : (mapped.isNotEmpty() ? mapped : "MIDI Learn"));
    bar.repaint();
}
} // namespace moshdeck
