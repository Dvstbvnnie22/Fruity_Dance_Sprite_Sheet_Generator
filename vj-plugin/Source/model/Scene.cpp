#include "Scene.h"
#include "render/EffectLibrary.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace moshdeck
{
//==============================================================================
juce::String sourceTypeName (SourceType t)
{
    switch (t)
    {
        case SourceType::None:      return "Empty";
        case SourceType::DeckA:     return "Deck A";
        case SourceType::DeckB:     return "Deck B";
        case SourceType::DeckMix:   return "Deck A/B Mix";
        case SourceType::Generator: return "Generator";
        case SourceType::Milk:      return "Milk Preset";
        case SourceType::AudioViz:  return "Audio Visualizer";
        case SourceType::Shadertoy: return "Shadertoy";
        case SourceType::Below:     return "Layers Below";
        default: break;
    }
    return "?";
}

juce::String inputRouteName (InputRoute r)
{
    switch (r)
    {
        case InputRoute::None:    return "None";
        case InputRoute::DeckA:   return "Deck A";
        case InputRoute::DeckB:   return "Deck B";
        case InputRoute::DeckMix: return "Deck Mix";
        case InputRoute::Below:   return "Layers Below";
        default: break;
    }
    return "?";
}

juce::String blendModeName (BlendMode m)
{
    static const char* names[] = { "Normal", "Add", "Screen", "Multiply", "Overlay", "Soft Light", "Hard Light",
                                   "Difference", "Exclusion", "Subtract", "Lighten", "Darken", "Color Dodge",
                                   "Color Burn", "Linear Light", "Hue", "Color", "Luminosity", "Luma Key",
                                   "Inverse Luma Key", "Alpha Over" };
    const int i = (int) m;
    return i >= 0 && i < (int) BlendMode::NumModes ? names[i] : "?";
}

juce::String vizTypeName (int t)
{
    static const char* names[numVizTypes] = { "Spectrum Bars", "Mirror Bars", "Circle Spectrum", "Circle Wave",
                                              "Waveform", "Spectrum Fill", "Particles", "Pulse Image",
                                              "Peak Bars", "Polygon Ring", "Dual Spectrum", "Glow Orb" };
    return t >= 0 && t < numVizTypes ? names[t] : "?";
}

juce::String modSourceName (ModSourceType t, int i, const juce::String& param)
{
    static const char* bands[] = { "Sub", "Bass", "Mid", "High-Mid", "Treble" };
    switch (t)
    {
        case ModSourceType::AudioBand:  return juce::String ("Audio ") + bands[juce::jlimit (0, 4, i)];
        case ModSourceType::AudioLevel: return "Audio Level";
        case ModSourceType::Kick:       return "Kick Onset";
        case ModSourceType::Snare:      return "Snare Onset";
        case ModSourceType::Hat:        return "Hat Onset";
        case ModSourceType::BeatRamp:   return "Beat Ramp";
        case ModSourceType::BarRamp:    return "Bar Ramp";
        case ModSourceType::BeatPulse:  return "Beat Pulse";
        case ModSourceType::LFO:        return "LFO " + juce::String (i + 1);
        case ModSourceType::RandomStep: return "Random / Beat";
        case ModSourceType::MidiCC:     return "MIDI Ch" + juce::String (i / 128 + 1) + " CC" + juce::String (i % 128);
        case ModSourceType::MidiNote:   return "MIDI Note " + juce::MidiMessage::getMidiNoteName (i, true, true, 3);
        case ModSourceType::Macro:      return "Macro " + juce::String (i + 1);
        case ModSourceType::Param:      return "Link: " + param;
        default: break;
    }
    return "?";
}

//==============================================================================
std::vector<ParamDef> sourceParamDefs (SourceType type, int idx)
{
    switch (type)
    {
        case SourceType::DeckA: case SourceType::DeckB: case SourceType::DeckMix: case SourceType::Below:
            return makeParams ({ { "Zoom", 0.25f, 4, 1 }, { "Pan X", -1, 1, 0 }, { "Pan Y", -1, 1, 0 },
                                 { "Rotate", -3.1416f, 3.1416f, 0 }, { "Bass Pulse", 0, 1, 0 }, { "Mirror Wrap", 0, 1, 0 } });
        case SourceType::Generator:
            if (auto* g = GeneratorLibrary::get (idx))
                return g->params;
            return {};
        case SourceType::Milk:
            return makeParams ({ { "Base Inject", 0, 1, 0.12f }, { "Inject Mode (0 const,1 beat,2 kick,3 luma)", 0, 3, 0 },
                                 { "Zoom Offset", -0.1f, 0.1f, 0 }, { "Rotation Offset", -0.1f, 0.1f, 0 },
                                 { "Warp Scale", 0, 3, 1 }, { "Decay Offset", -0.1f, 0.05f, 0 },
                                 { "Wave Scale", 0, 3, 1 }, { "Auto Next (beats, 0 off)", 0, 64, 0 } });
        case SourceType::AudioViz:
            return makeParams ({ { "Size", 0.1f, 3, 1 }, { "X", -1, 1, 0 }, { "Y", -1, 1, 0 }, { "Bass React", 0, 2, 0.5f },
                                 { "Hue", 0, 1, 0.55f }, { "Hue Spread", 0, 1, 0.5f },
                                 { "Detail / Count", 4, 160, idx == 6 ? 90.0f : 64.0f }, { "Thickness / Glow", 0, 2, 0.5f } });
        case SourceType::Shadertoy:
            return makeParams ({ { "iParam0", 0, 1, 0.5f }, { "iParam1", 0, 1, 0.5f }, { "iParam2", 0, 1, 0.5f }, { "iParam3", 0, 1, 0.5f },
                                 { "iMouse X", 0, 1, 0.5f }, { "iMouse Y", 0, 1, 0.5f }, { "iMouse Down", 0, 1, 0 } });
        default: break;
    }
    return {};
}

//==============================================================================
void SceneData::rebuildIndex()
{
    index.clear();
    for (size_t i = 0; i < params.size(); ++i)
        index[params[i].id] = (int) i;
}

Scene::Scene() { resetToDefault(); }

void Scene::resetToDefault()
{
    {
        const juce::ScopedLock sl (lock);
        data = SceneData();
        uidCounter = 1;
        addGlobalParams();
    }
    auto a = addLayer (SourceType::DeckMix);
    auto m = addLayer (SourceType::Milk);
    editLayer (m, [] (Layer& l) { l.milkPreset = "builtin:Liquid Mirror"; l.blendMode = (int) BlendMode::Screen; });
    setValue (m + ".opacity", 0.8f);
    juce::ignoreUnused (a);
    bump();
}

void Scene::addGlobalParams()
{
    addParam (crossfaderId, "Crossfader A/B", "Crossfader", 0, 1, 0);
    addParam ("G.xfade.mode", "Crossfader Mode (0 Fade, 1 Cut)", "Crossfader", 0, 1, 0);
    addParam ("G.xfade.curve", "Crossfader Curve (0 Linear, 1 Smooth, 2 Sharp A, 3 Sharp B)", "Crossfader", 0, 3, 0);
    addParam ("G.xfade.rev", "Crossfader Reverse (hamster / inverse)", "Crossfader", 0, 1, 0);
    addParam ("G.xfade.cut", "Crossfader Cut Point", "Crossfader", 0, 1, 0.5f);
    addParam ("G.xfade.smooth", "Crossfader Smoothing (ms)", "Crossfader", 0, 500, 0);
    addParam (masterId, "Master Brightness", "Global", 0, 2, 1);
    const float divs[] = { 1, 2, 4, 8 };
    for (int i = 0; i < 4; ++i)
    {
        const juce::String n (i + 1);
        addParam ("G.lfo" + n + ".div", "LFO " + n + " Beats/Cycle", "LFOs", 0.0625f, 32, divs[i]);
        addParam ("G.lfo" + n + ".shape", "LFO " + n + " Shape (sin,tri,saw,sqr,s&h)", "LFOs", 0, 4, 0);
    }
    for (int d = 0; d < 2; ++d)
    {
        const juce::String p = d == 0 ? "DA" : "DB", n = d == 0 ? "Deck A" : "Deck B";
        addParam (p + ".speed", n + " Speed", n, -2, 4, 1);
        addParam (p + ".beats", n + " Beat Loop (0 off)", n, 0, 32, 0);
        addParam (p + ".cue", n + " Cue Offset", n, 0, 1, 0);
        addParam (p + ".next", n + " Next Clip Every N Bars (0 off)", n, 0, 32, 0);
    }
}

void Scene::addParam (const juce::String& id, const juce::String& name, const juce::String& group, float mn, float mx, float def)
{
    if (data.find (id) >= 0)
        return;
    data.params.push_back ({ id, name, group, mn, mx, def });
    data.values.push_back (def);
    data.index[id] = (int) data.params.size() - 1;
}

void Scene::removeParamsWithPrefix (const juce::String& prefix)
{
    std::vector<Param> p; std::vector<float> v;
    for (size_t i = 0; i < data.params.size(); ++i)
        if (! data.params[i].id.startsWith (prefix))
        {
            p.push_back (data.params[i]);
            v.push_back (data.values[i]);
        }
    data.params = std::move (p);
    data.values = std::move (v);
    data.rebuildIndex();
    // drop mod routes whose destination vanished
    data.routes.erase (std::remove_if (data.routes.begin(), data.routes.end(),
                                       [&] (const ModRoute& r) { return r.dest.startsWith (prefix) || (r.source == ModSourceType::Param && r.sourceParam.startsWith (prefix)); }),
                       data.routes.end());
}

juce::String Scene::nextUid (const char* prefix) { return juce::String (prefix) + juce::String (uidCounter++); }

juce::String Scene::layerGroupName (const Layer& l) const { return l.name; }

void Scene::rebuildSourceParams (Layer& l)
{
    removeParamsWithPrefix (l.uid + ".p");
    const auto defs = sourceParamDefs (l.source, l.sourceIndex);
    for (auto& d : defs)
        addParam (l.uid + "." + d.key, d.name, layerGroupName (l), d.min, d.max, d.def);
}

void Scene::addEffectParams (const juce::String& layerUid, const EffectSlot& fx)
{
    if (auto* def = EffectLibrary::find (fx.effectId))
    {
        const juce::String base = layerUid + "." + fx.uid + ".";
        const juce::String group = (layerUid == "master" ? juce::String ("Master") : layerUid) + " / " + def->name;
        addParam (base + "mix", def->name + " Mix", group, 0, 1, 1);
        for (auto& d : def->params)
            addParam (base + d.key, d.name, group, d.min, d.max, d.def);
    }
}

//==============================================================================
uint32_t Scene::snapshot (SceneData& dest, uint32_t knownVersion) const
{
    const juce::ScopedLock sl (lock);
    const auto v = version.load();
    if (v != knownVersion)
        dest = data;
    else
        dest.values = data.values;
    return v;
}

void Scene::publishModulated (const std::vector<float>& finalValues, uint32_t v)
{
    const juce::ScopedTryLock sl (lock);
    if (sl.isLocked() && v == version.load())
    {
        modulated = finalValues;
        modulatedVersion = v;
    }
}

float Scene::getModulated (const juce::String& id) const
{
    const juce::ScopedLock sl (lock);
    const int i = data.find (id);
    if (i < 0) return 0.0f;
    if (modulatedVersion == version.load() && (size_t) i < modulated.size())
        return modulated[(size_t) i];
    return data.values[(size_t) i];
}

float Scene::getValue (const juce::String& id) const
{
    const juce::ScopedLock sl (lock);
    return data.get (id);
}

void Scene::setValue (const juce::String& id, float v)
{
    const juce::ScopedLock sl (lock);
    const int i = data.find (id);
    if (i >= 0)
    {
        auto& p = data.params[(size_t) i];
        data.values[(size_t) i] = juce::jlimit (p.min, p.max, v);
    }
}

void Scene::setNormalised (const juce::String& id, float n)
{
    const juce::ScopedLock sl (lock);
    const int i = data.find (id);
    if (i >= 0)
    {
        auto& p = data.params[(size_t) i];
        data.values[(size_t) i] = p.min + juce::jlimit (0.0f, 1.0f, n) * (p.max - p.min);
    }
}

float Scene::getNormalised (const juce::String& id) const
{
    const juce::ScopedLock sl (lock);
    const int i = data.find (id);
    if (i < 0) return 0.0f;
    auto& p = data.params[(size_t) i];
    return p.max > p.min ? (data.values[(size_t) i] - p.min) / (p.max - p.min) : 0.0f;
}

bool Scene::getParam (const juce::String& id, Param& out) const
{
    const juce::ScopedLock sl (lock);
    const int i = data.find (id);
    if (i < 0) return false;
    out = data.params[(size_t) i];
    return true;
}

std::vector<Param> Scene::getAllParams() const
{
    const juce::ScopedLock sl (lock);
    return data.params;
}

//==============================================================================
Layer* Scene::findLayer (const juce::String& uid)
{
    for (auto& l : data.layers)
        if (l.uid == uid)
            return &l;
    return nullptr;
}

std::vector<EffectSlot>* Scene::findChain (const juce::String& layerUid)
{
    if (layerUid == "master")
        return &data.masterFx;
    if (auto* l = findLayer (layerUid))
        return &l->effects;
    return nullptr;
}

juce::String Scene::addLayer (SourceType type, int sourceIndex)
{
    const juce::ScopedLock sl (lock);
    Layer l;
    l.uid = nextUid ("L");
    l.source = type;
    l.sourceIndex = sourceIndex;
    if (type == SourceType::Milk)
        l.milkPreset = "builtin:Liquid Mirror";
    l.name = "Layer " + l.uid.substring (1) + " - " + sourceTypeName (type);
    if (type == SourceType::Generator)
        if (auto* g = GeneratorLibrary::get (sourceIndex))
            l.name = "Layer " + l.uid.substring (1) + " - " + g->name;
    if (type == SourceType::AudioViz)
    {
        l.name = "Layer " + l.uid.substring (1) + " - " + vizTypeName (sourceIndex);
        l.blendMode = (int) BlendMode::AlphaOver;
    }
    if (data.layers.size() > 0 && type != SourceType::Below)
        l.blendMode = type == SourceType::AudioViz ? (int) BlendMode::AlphaOver : (int) BlendMode::Normal;

    addParam (l.uid + ".opacity", "Opacity", layerGroupName (l), 0, 1, 1);
    addParam (l.uid + ".speed", "Time Speed", layerGroupName (l), 0, 4, 1);
    data.layers.push_back (l);
    rebuildSourceParams (data.layers.back());
    bump();
    return l.uid;
}

void Scene::removeLayer (const juce::String& uid)
{
    const juce::ScopedLock sl (lock);
    data.layers.erase (std::remove_if (data.layers.begin(), data.layers.end(), [&] (const Layer& l) { return l.uid == uid; }), data.layers.end());
    removeParamsWithPrefix (uid + ".");
    bump();
}

void Scene::moveLayer (const juce::String& uid, int delta)
{
    const juce::ScopedLock sl (lock);
    for (size_t i = 0; i < data.layers.size(); ++i)
        if (data.layers[i].uid == uid)
        {
            const int j = juce::jlimit (0, (int) data.layers.size() - 1, (int) i + delta);
            std::swap (data.layers[i], data.layers[(size_t) j]);
            break;
        }
    bump();
}

void Scene::duplicateLayer (const juce::String& uid)
{
    const juce::ScopedLock sl (lock);
    auto* src = findLayer (uid);
    if (src == nullptr) return;
    Layer copy = *src;
    copy.uid = nextUid ("L");
    copy.name = src->name + " copy";
    std::vector<std::pair<juce::String, float>> vals;
    for (size_t i = 0; i < data.params.size(); ++i)
        if (data.params[i].id.startsWith (uid + "."))
        {
            auto p = data.params[i];
            const juce::String newId = copy.uid + p.id.substring (uid.length());
            vals.push_back ({ newId, data.values[i] });
            p.id = newId;
            data.params.push_back (p);
            data.values.push_back (data.values[i]);
        }
    for (auto& fx : copy.effects)
    {
        const auto oldUid = fx.uid;
        fx.uid = nextUid ("fx");
        for (auto& p : data.params)
            if (p.id.startsWith (copy.uid + "." + oldUid + "."))
                p.id = copy.uid + "." + fx.uid + "." + p.id.fromFirstOccurrenceOf (oldUid + ".", false, false);
    }
    data.rebuildIndex();
    auto it = std::find_if (data.layers.begin(), data.layers.end(), [&] (const Layer& l) { return l.uid == uid; });
    data.layers.insert (it + 1, copy);
    bump();
}

bool Scene::getLayer (const juce::String& uid, Layer& out) const
{
    const juce::ScopedLock sl (lock);
    for (auto& l : data.layers)
        if (l.uid == uid) { out = l; return true; }
    return false;
}

std::vector<Layer> Scene::getLayers() const
{
    const juce::ScopedLock sl (lock);
    return data.layers;
}

void Scene::setLayerSource (const juce::String& uid, SourceType type, int index)
{
    editLayer (uid, [&] (Layer& l)
    {
        l.source = type;
        l.sourceIndex = index;
        if (type == SourceType::Milk && l.milkPreset.isEmpty())
            l.milkPreset = "builtin:Liquid Mirror";
        juce::String detail = sourceTypeName (type);
        if (type == SourceType::Generator)
            if (auto* g = GeneratorLibrary::get (index)) detail = g->name;
        if (type == SourceType::AudioViz)
            detail = vizTypeName (index);
        l.name = "Layer " + l.uid.substring (1) + " - " + detail;
    });
    const juce::ScopedLock sl (lock);
    if (auto* l = findLayer (uid))
        rebuildSourceParams (*l); // ensure defaults even if type/index unchanged
    bump();
}

//==============================================================================
juce::String Scene::addEffect (const juce::String& layerUid, const juce::String& effectId)
{
    const juce::ScopedLock sl (lock);
    auto* chain = findChain (layerUid);
    if (chain == nullptr || EffectLibrary::find (effectId) == nullptr)
        return {};
    EffectSlot fx;
    fx.uid = nextUid ("fx");
    fx.effectId = effectId;
    chain->push_back (fx);
    addEffectParams (layerUid, fx);
    bump();
    return fx.uid;
}

void Scene::removeEffect (const juce::String& layerUid, const juce::String& fxUid)
{
    const juce::ScopedLock sl (lock);
    if (auto* chain = findChain (layerUid))
        chain->erase (std::remove_if (chain->begin(), chain->end(), [&] (const EffectSlot& e) { return e.uid == fxUid; }), chain->end());
    removeParamsWithPrefix (layerUid + "." + fxUid + ".");
    bump();
}

void Scene::moveEffect (const juce::String& layerUid, const juce::String& fxUid, int delta)
{
    const juce::ScopedLock sl (lock);
    if (auto* chain = findChain (layerUid))
        for (size_t i = 0; i < chain->size(); ++i)
            if ((*chain)[i].uid == fxUid)
            {
                const int j = juce::jlimit (0, (int) chain->size() - 1, (int) i + delta);
                std::swap ((*chain)[i], (*chain)[(size_t) j]);
                break;
            }
    bump();
}

void Scene::setEffectEnabled (const juce::String& layerUid, const juce::String& fxUid, bool on)
{
    const juce::ScopedLock sl (lock);
    if (auto* chain = findChain (layerUid))
        for (auto& e : *chain)
            if (e.uid == fxUid) e.enabled = on;
    bump();
}

std::vector<EffectSlot> Scene::getEffects (const juce::String& layerUid) const
{
    const juce::ScopedLock sl (lock);
    if (auto* chain = const_cast<Scene*> (this)->findChain (layerUid))
        return *chain;
    return {};
}

//==============================================================================
juce::String Scene::addRoute (const ModRoute& r, float amount)
{
    const juce::ScopedLock sl (lock);
    ModRoute route = r;
    route.uid = nextUid ("m");
    juce::String destName = r.dest;
    const int di = data.find (r.dest);
    if (di >= 0) destName = data.params[(size_t) di].group + " / " + data.params[(size_t) di].name;
    addParam (route.uid + ".amt", modSourceName (r.source, r.sourceIndex, r.sourceParam) + " -> " + destName, "Modulation", -1, 1, amount);
    data.routes.push_back (route);
    bump();
    return route.uid;
}

void Scene::removeRoute (const juce::String& uid)
{
    const juce::ScopedLock sl (lock);
    data.routes.erase (std::remove_if (data.routes.begin(), data.routes.end(), [&] (const ModRoute& r) { return r.uid == uid; }), data.routes.end());
    removeParamsWithPrefix (uid + ".");
    bump();
}

void Scene::updateRoute (const ModRoute& r)
{
    const juce::ScopedLock sl (lock);
    for (auto& x : data.routes)
        if (x.uid == r.uid) x = r;
    bump();
}

std::vector<ModRoute> Scene::getRoutes() const
{
    const juce::ScopedLock sl (lock);
    return data.routes;
}

std::vector<ModRoute> Scene::getRoutesTo (const juce::String& dest) const
{
    const juce::ScopedLock sl (lock);
    std::vector<ModRoute> out;
    for (auto& r : data.routes)
        if (r.dest == dest) out.push_back (r);
    return out;
}

DeckState Scene::getDeck (int d) const
{
    const juce::ScopedLock sl (lock);
    return data.decks[juce::jlimit (0, 1, d)];
}

void Scene::setDeck (int d, const DeckState& s)
{
    const juce::ScopedLock sl (lock);
    data.decks[juce::jlimit (0, 1, d)] = s;
    bump();
}

//==============================================================================
static juce::ValueTree fxToTree (const EffectSlot& fx)
{
    juce::ValueTree t ("FX");
    t.setProperty ("uid", fx.uid, nullptr);
    t.setProperty ("effect", fx.effectId, nullptr);
    t.setProperty ("enabled", fx.enabled, nullptr);
    return t;
}

juce::ValueTree Scene::toValueTree() const
{
    const juce::ScopedLock sl (lock);
    juce::ValueTree root ("MoshDeckScene");
    root.setProperty ("version", 1, nullptr);
    root.setProperty ("uidCounter", uidCounter, nullptr);

    for (auto& l : data.layers)
    {
        juce::ValueTree t ("Layer");
        t.setProperty ("uid", l.uid, nullptr);
        t.setProperty ("name", l.name, nullptr);
        t.setProperty ("source", (int) l.source, nullptr);
        t.setProperty ("input", (int) l.input, nullptr);
        t.setProperty ("index", l.sourceIndex, nullptr);
        t.setProperty ("milk", l.milkPreset, nullptr);
        t.setProperty ("milkFolder", l.milkFolder, nullptr);
        t.setProperty ("shader", l.shaderCode, nullptr);
        t.setProperty ("shaderName", l.shaderName, nullptr);
        t.setProperty ("blend", l.blendMode, nullptr);
        t.setProperty ("enabled", l.enabled, nullptr);
        t.setProperty ("solo", l.solo, nullptr);
        for (auto& fx : l.effects) t.appendChild (fxToTree (fx), nullptr);
        root.appendChild (t, nullptr);
    }
    juce::ValueTree master ("Master");
    for (auto& fx : data.masterFx) master.appendChild (fxToTree (fx), nullptr);
    root.appendChild (master, nullptr);

    for (auto& r : data.routes)
    {
        juce::ValueTree t ("Route");
        t.setProperty ("uid", r.uid, nullptr);
        t.setProperty ("source", (int) r.source, nullptr);
        t.setProperty ("index", r.sourceIndex, nullptr);
        t.setProperty ("param", r.sourceParam, nullptr);
        t.setProperty ("dest", r.dest, nullptr);
        t.setProperty ("smooth", r.smoothing, nullptr);
        t.setProperty ("bipolar", r.bipolar, nullptr);
        root.appendChild (t, nullptr);
    }
    for (int d = 0; d < 2; ++d)
    {
        juce::ValueTree t ("Deck");
        t.setProperty ("index", d, nullptr);
        t.setProperty ("current", data.decks[d].current, nullptr);
        for (auto& f : data.decks[d].files)
        {
            juce::ValueTree ft ("File");
            ft.setProperty ("path", f, nullptr);
            t.appendChild (ft, nullptr);
        }
        root.appendChild (t, nullptr);
    }
    juce::ValueTree vals ("Values");
    for (size_t i = 0; i < data.params.size(); ++i)
    {
        juce::ValueTree p ("P");
        p.setProperty ("id", data.params[i].id, nullptr);
        p.setProperty ("v", data.values[i], nullptr);
        vals.appendChild (p, nullptr);
    }
    root.appendChild (vals, nullptr);
    return root;
}

void Scene::fromValueTree (const juce::ValueTree& root)
{
    if (! root.hasType ("MoshDeckScene"))
        return;
    const juce::ScopedLock sl (lock);
    data = SceneData();
    addGlobalParams();

    auto readFx = [this] (const juce::ValueTree& parent, const juce::String& owner, std::vector<EffectSlot>& chain)
    {
        for (auto c : parent)
            if (c.hasType ("FX"))
            {
                EffectSlot fx;
                fx.uid = c["uid"].toString();
                fx.effectId = c["effect"].toString();
                fx.enabled = (bool) c.getProperty ("enabled", true);
                if (EffectLibrary::find (fx.effectId) == nullptr) continue;
                chain.push_back (fx);
                addEffectParams (owner, fx);
            }
    };

    for (auto t : root)
    {
        if (t.hasType ("Layer"))
        {
            Layer l;
            l.uid = t["uid"].toString();
            l.name = t["name"].toString();
            l.source = (SourceType) (int) t["source"];
            l.input = (InputRoute) (int) t.getProperty ("input", (int) InputRoute::DeckMix);
            l.sourceIndex = t["index"];
            l.milkPreset = t["milk"].toString();
            l.milkFolder = t["milkFolder"].toString();
            l.shaderCode = t["shader"].toString();
            l.shaderName = t["shaderName"].toString();
            l.blendMode = t["blend"];
            l.enabled = (bool) t.getProperty ("enabled", true);
            l.solo = (bool) t.getProperty ("solo", false);
            addParam (l.uid + ".opacity", "Opacity", l.name, 0, 1, 1);
            addParam (l.uid + ".speed", "Time Speed", l.name, 0, 4, 1);
            for (auto& d : sourceParamDefs (l.source, l.sourceIndex))
                addParam (l.uid + "." + d.key, d.name, l.name, d.min, d.max, d.def);
            readFx (t, l.uid, l.effects);
            data.layers.push_back (l);
        }
        else if (t.hasType ("Master"))
            readFx (t, "master", data.masterFx);
        else if (t.hasType ("Deck"))
        {
            auto& d = data.decks[juce::jlimit (0, 1, (int) t["index"])];
            d.current = t["current"];
            for (auto f : t)
                d.files.add (f["path"].toString());
        }
    }
    for (auto t : root)
        if (t.hasType ("Route"))
        {
            ModRoute r;
            r.uid = t["uid"].toString();
            r.source = (ModSourceType) (int) t["source"];
            r.sourceIndex = t["index"];
            r.sourceParam = t["param"].toString();
            r.dest = t["dest"].toString();
            r.smoothing = t["smooth"];
            r.bipolar = t["bipolar"];
            addParam (r.uid + ".amt", modSourceName (r.source, r.sourceIndex, r.sourceParam) + " -> " + r.dest, "Modulation", -1, 1, 0.5f);
            data.routes.push_back (r);
        }
    auto vals = root.getChildWithName ("Values");
    for (auto p : vals)
    {
        const int i = data.find (p["id"].toString());
        if (i >= 0) data.values[(size_t) i] = juce::jlimit (data.params[(size_t) i].min, data.params[(size_t) i].max, (float) p["v"]);
    }
    uidCounter = juce::jmax ((int) root.getProperty ("uidCounter", 1), 1);
    bump();
}
} // namespace moshdeck
