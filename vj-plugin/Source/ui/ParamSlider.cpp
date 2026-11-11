#include "ParamSlider.h"
#include "Theme.h"
#include "PluginProcessor.h"

namespace moshdeck
{
struct SourceItem { ModSourceType type; int index; };

static std::vector<SourceItem> sourceItems()
{
    std::vector<SourceItem> v;
    for (int i = 0; i < 5; ++i) v.push_back ({ ModSourceType::AudioBand, i });
    v.push_back ({ ModSourceType::AudioLevel, 0 });
    v.push_back ({ ModSourceType::Kick, 0 });
    v.push_back ({ ModSourceType::Snare, 0 });
    v.push_back ({ ModSourceType::Hat, 0 });
    v.push_back ({ ModSourceType::BeatRamp, 0 });
    v.push_back ({ ModSourceType::BarRamp, 0 });
    v.push_back ({ ModSourceType::BeatPulse, 0 });
    for (int i = 0; i < 4; ++i) v.push_back ({ ModSourceType::LFO, i });
    v.push_back ({ ModSourceType::RandomStep, 0 });
    for (int i = 0; i < 16; ++i) v.push_back ({ ModSourceType::Macro, i });
    return v;
}

void ModSourceMenu::add (juce::PopupMenu& m, int baseId)
{
    const auto items = sourceItems();
    juce::PopupMenu audio, beat, lfo, macro;
    for (size_t i = 0; i < items.size(); ++i)
    {
        const auto& it = items[i];
        const auto name = modSourceName (it.type, it.index);
        const int id = baseId + (int) i;
        switch (it.type)
        {
            case ModSourceType::AudioBand: case ModSourceType::AudioLevel: case ModSourceType::Kick:
            case ModSourceType::Snare: case ModSourceType::Hat: audio.addItem (id, name); break;
            case ModSourceType::BeatRamp: case ModSourceType::BarRamp: case ModSourceType::BeatPulse:
            case ModSourceType::RandomStep: beat.addItem (id, name); break;
            case ModSourceType::LFO: lfo.addItem (id, name + " (beat-synced)"); break;
            case ModSourceType::Macro: macro.addItem (id, name + " (host automation)"); break;
            default: break;
        }
    }
    m.addSubMenu ("Audio reactive", audio);
    m.addSubMenu ("Beat / tempo", beat);
    m.addSubMenu ("LFOs", lfo);
    m.addSubMenu ("Macros", macro);
}

bool ModSourceMenu::decode (int id, int baseId, ModSourceType& type, int& index)
{
    const auto items = sourceItems();
    const int i = id - baseId;
    if (i < 0 || i >= (int) items.size()) return false;
    type = items[(size_t) i].type;
    index = items[(size_t) i].index;
    return true;
}

void ParamMenu::add (juce::PopupMenu& m, const std::vector<Param>& params, int baseId, const juce::String& exclude)
{
    std::map<juce::String, juce::PopupMenu> groups;
    juce::StringArray order;
    for (size_t i = 0; i < params.size(); ++i)
    {
        if (params[i].id == exclude) continue;
        order.addIfNotAlreadyThere (params[i].group);
        groups[params[i].group].addItem (baseId + (int) i, params[i].name);
    }
    for (auto& g : order) m.addSubMenu (g, groups[g]);
}

//==============================================================================
ParamSlider::ParamSlider (MoshDeckProcessor& p, const juce::String& paramId, const juce::String& labelOverride)
    : proc (p), id (paramId)
{
    proc.scene.getParam (id, def);
    label.setText (labelOverride.isNotEmpty() ? labelOverride : def.name, juce::dontSendNotification);
    label.setFont (juce::FontOptions (12.0f));
    label.setColour (juce::Label::textColourId, theme::dim);
    label.setMinimumHorizontalScale (0.6f);
    addAndMakeVisible (label);

    slider.setSliderStyle (juce::Slider::LinearBar);
    slider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 0, 0);
    slider.setColour (juce::Slider::trackColourId, theme::accent.withAlpha (0.45f));
    slider.setColour (juce::Slider::textBoxTextColourId, theme::text);
    const double range = def.max - def.min;
    slider.setRange (def.min, def.max, range > 20 ? 0.1 : (range > 2 ? 0.01 : 0.001));
    slider.setDoubleClickReturnValue (true, def.def);
    const int decimals = range > 20 ? 1 : (range > 2 ? 2 : 3);
    slider.textFromValueFunction = [decimals] (double v) { return juce::String (std::abs (v) < 0.5 * std::pow (10.0, -decimals) ? 0.0 : v, decimals); };
    slider.setValue (proc.scene.getValue (id), juce::dontSendNotification);
    slider.onValueChange = [this] { proc.scene.setValue (id, (float) slider.getValue()); };
    slider.onPopup = [this] { showMenu (proc, id, this); };
    addAndMakeVisible (slider);
    startTimerHz (20);
}

void ParamSlider::resized()
{
    auto r = getLocalBounds().reduced (1);
    label.setBounds (r.removeFromLeft (juce::jmin (150, r.getWidth() * 45 / 100)));
    slider.setBounds (r);
}

void ParamSlider::timerCallback()
{
    if (! slider.isMouseButtonDown())
    {
        const float v = proc.scene.getValue (id);
        if (std::abs (v - (float) slider.getValue()) > 1.0e-6f)
            slider.setValue (v, juce::dontSendNotification);
    }
    const float m = proc.scene.getModulated (id);
    const bool hm = ! proc.scene.getRoutesTo (id).empty();
    const bool ar = proc.midiLearn.getArmed() == id;
    const auto mt = proc.midiLearn.describeMapping (id);
    if (m != modulated || hm != hasMod || ar != armed || mt != midiText)
    {
        modulated = m; hasMod = hm; armed = ar; midiText = mt;
        slider.setTooltip (def.group + " / " + def.name + (mt.isNotEmpty() ? "\nMIDI: " + mt : juce::String())
                           + "\nRight-click: MIDI learn, modulate, link");
        repaint();
    }
}

void ParamSlider::paintOverChildren (juce::Graphics& g)
{
    auto sb = slider.getBounds().toFloat();
    if (hasMod && def.max > def.min)
    {
        const float n = juce::jlimit (0.0f, 1.0f, (modulated - def.min) / (def.max - def.min));
        g.setColour (theme::mod);
        g.fillRect (sb.getX() + n * sb.getWidth() - 1.5f, sb.getY(), 3.0f, sb.getHeight());
    }
    if (midiText.isNotEmpty())
    {
        g.setColour (theme::midi);
        g.fillEllipse (sb.getRight() - 8, sb.getY() + 2, 5, 5);
    }
    if (armed)
    {
        g.setColour (theme::midi);
        g.drawRect (getLocalBounds(), 2);
    }
}

void ParamSlider::showMenu (MoshDeckProcessor& proc, const juce::String& id, juce::Component* target)
{
    juce::PopupMenu m;
    const auto mapping = proc.midiLearn.describeMapping (id);
    m.addSectionHeader (id);
    m.addItem (1, "MIDI Learn" + juce::String (mapping.isNotEmpty() ? " (now: " + mapping + ")" : ""));
    m.addItem (2, "Clear MIDI mapping", mapping.isNotEmpty());
    m.addSeparator();
    juce::PopupMenu mod;
    ModSourceMenu::add (mod, 1000);
    m.addSubMenu ("Modulate with", mod);
    m.addItem (3, "Modulate with MIDI (move a control...)");
    const auto params = proc.scene.getAllParams();
    juce::PopupMenu link;
    ParamMenu::add (link, params, 100000, id);
    m.addSubMenu ("Link to parameter (follows its value)", link);
    const auto routes = proc.scene.getRoutesTo (id);
    m.addItem (4, "Remove modulation (" + juce::String ((int) routes.size()) + ")", ! routes.empty());
    m.addSeparator();
    m.addItem (5, "Reset to default");

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target),
                     [&proc, id, params] (int r)
    {
        if (r == 1) proc.midiLearn.arm (id);
        else if (r == 2) proc.midiLearn.removeMappingsFor (id);
        else if (r == 3) proc.midiLearn.arm ("R." + id);
        else if (r == 4) { for (auto& rt : proc.scene.getRoutesTo (id)) proc.scene.removeRoute (rt.uid); }
        else if (r == 5) { Param p; if (proc.scene.getParam (id, p)) proc.scene.setValue (id, p.def); }
        else if (r >= 100000)
        {
            const int i = r - 100000;
            if (i >= 0 && i < (int) params.size())
            {
                ModRoute rt;
                rt.source = ModSourceType::Param;
                rt.sourceParam = params[(size_t) i].id;
                rt.dest = id;
                proc.scene.addRoute (rt, 0.5f);
            }
        }
        else if (r >= 1000)
        {
            ModRoute rt;
            if (ModSourceMenu::decode (r, 1000, rt.source, rt.sourceIndex))
            {
                rt.dest = id;
                proc.scene.addRoute (rt, 0.3f);
            }
        }
    });
}

//==============================================================================
ParamPanel::ParamPanel (MoshDeckProcessor& p, const juce::StringArray& ids)
{
    for (auto& i : ids)
        addAndMakeVisible (sliders.add (new ParamSlider (p, i)));
}

void ParamPanel::resized()
{
    auto r = getLocalBounds();
    for (auto* s : sliders) s->setBounds (r.removeFromTop (24));
}
} // namespace moshdeck
