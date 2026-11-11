#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "model/Scene.h"

class MoshDeckProcessor;

namespace moshdeck
{
/** Builds the "Modulate with..." menu; returns (type,index) for chosen item ids. */
struct ModSourceMenu
{
    static void add (juce::PopupMenu& m, int baseId);
    static bool decode (int id, int baseId, ModSourceType& type, int& index);
};

/** Parameter-chooser menu grouped by Param::group. */
struct ParamMenu
{
    static void add (juce::PopupMenu& m, const std::vector<Param>& params, int baseId, const juce::String& exclude = {});
};

/**
    A slider bound to one Scene parameter, with the right-click menu that makes
    everything performable: MIDI Learn, modulation from audio / beat / LFO / MIDI,
    and linking to any other parameter. Shows the live modulated value.
*/
class ParamSlider : public juce::Component, private juce::Timer
{
public:
    ParamSlider (MoshDeckProcessor& p, const juce::String& paramId, const juce::String& labelOverride = {});
    void resized() override;
    void paintOverChildren (juce::Graphics&) override;
    const juce::String& getParamId() const { return id; }

    static void showMenu (MoshDeckProcessor& p, const juce::String& paramId, juce::Component* target);

private:
    struct S : juce::Slider
    {
        std::function<void()> onPopup;
        void mouseDown (const juce::MouseEvent& e) override { if (e.mods.isPopupMenu()) { if (onPopup) onPopup(); return; } juce::Slider::mouseDown (e); }
    };
    void timerCallback() override;

    MoshDeckProcessor& proc;
    juce::String id;
    Param def;
    S slider;
    juce::Label label;
    float modulated = 0.0f;
    bool hasMod = false, armed = false;
    juce::String midiText;
};

/** A vertical stack of ParamSliders. */
class ParamPanel : public juce::Component
{
public:
    ParamPanel (MoshDeckProcessor& p, const juce::StringArray& ids);
    void resized() override;
    int getIdealHeight() const { return sliders.size() * 24; }
private:
    juce::OwnedArray<ParamSlider> sliders;
};
} // namespace moshdeck
