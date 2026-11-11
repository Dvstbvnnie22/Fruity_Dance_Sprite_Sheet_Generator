#pragma once
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <vector>
#include <unordered_map>

namespace moshdeck
{
//==============================================================================
enum class SourceType : int
{
    None = 0,
    DeckA,       // main media track A
    DeckB,       // main media track B
    DeckMix,     // A/B crossfaded by the global crossfader
    Generator,   // built-in generative shader
    Milk,        // MilkDrop-format preset engine (feedback/warp visuals)
    AudioViz,    // spectrum / waveform / particle visualizer element
    Shadertoy,   // imported Shadertoy shader (single or multipass)
    Below,       // composite of all layers underneath (for "adjustment" layers)
    NumTypes
};

juce::String sourceTypeName (SourceType);

/** Which picture a generative source uses as its "base" input (uInput / iChannel0). */
enum class InputRoute : int { None = 0, DeckA, DeckB, DeckMix, Below, NumRoutes };
juce::String inputRouteName (InputRoute);

enum class BlendMode : int
{
    Normal = 0, Add, Screen, Multiply, Overlay, SoftLight, HardLight, Difference, Exclusion,
    Subtract, Lighten, Darken, ColorDodge, ColorBurn, LinearLight, Hue, Color, Luminosity,
    LumaKey, InverseLumaKey, AlphaOver, NumModes
};
juce::String blendModeName (BlendMode);

//==============================================================================
struct ParamDef
{
    juce::String key;   // local key, e.g. "p0", "mix"
    juce::String name;  // display name
    float min = 0.0f, max = 1.0f, def = 0.0f;
};

struct Param
{
    juce::String id;    // globally unique, e.g. "L3.fx7.p2"
    juce::String name;  // display name
    juce::String group; // display group, e.g. "Layer 3 / RGB Split"
    float min = 0.0f, max = 1.0f, def = 0.0f;
};

struct EffectSlot
{
    juce::String uid;       // e.g. "fx7"
    juce::String effectId;  // key into EffectLibrary
    bool enabled = true;
};

struct Layer
{
    juce::String uid;  // e.g. "L3"
    juce::String name;
    SourceType source = SourceType::DeckA;
    InputRoute input  = InputRoute::DeckMix;
    int sourceIndex = 0;            // generator / visualizer type index
    juce::String milkPreset;        // "builtin:<name>" or absolute file path
    juce::String milkFolder;        // optional folder for preset auto-advance
    juce::String shaderCode;        // Shadertoy GLSL or JSON export
    juce::String shaderName;
    int blendMode = 0;
    bool enabled = true;
    bool solo = false;
    std::vector<EffectSlot> effects;
};

struct DeckState
{
    juce::StringArray files;
    int current = 0;
};

//==============================================================================
enum class ModSourceType : int
{
    AudioBand = 0,  // index 0..4 (sub, bass, mid, hi-mid, treble)
    AudioLevel,
    Kick, Snare, Hat,
    BeatRamp,       // 0..1 saw every beat
    BarRamp,        // 0..1 saw every bar
    BeatPulse,      // 1 on the beat, decays
    LFO,            // index 0..3, beat-synced
    RandomStep,     // new random value each beat
    MidiCC,         // index = channel*128 + cc
    MidiNote,       // index = note (velocity while held)
    Macro,          // index 0..15 (host-automatable)
    Param,          // another parameter's final value (normalised) - "link"
    NumTypes
};

struct ModRoute
{
    juce::String uid;         // e.g. "m4"; amount lives in param "<uid>.amt"
    ModSourceType source = ModSourceType::AudioBand;
    int sourceIndex = 0;
    juce::String sourceParam; // when source == Param
    juce::String dest;        // destination param id
    float smoothing = 0.0f;   // 0..0.99 one-pole
    bool bipolar = false;     // source mapped to -1..1 instead of 0..1
};

juce::String modSourceName (ModSourceType, int index, const juce::String& param = {});

//==============================================================================
/** Plain-data copy of the scene; the render thread works on one of these. */
struct SceneData
{
    std::vector<Layer> layers;       // bottom -> top
    std::vector<EffectSlot> masterFx;
    std::vector<ModRoute> routes;
    std::vector<Param> params;
    std::vector<float> values;       // base values, parallel to params
    DeckState decks[2];
    std::unordered_map<juce::String, int> index; // param id -> index

    int find (const juce::String& id) const
    {
        auto it = index.find (id);
        return it == index.end() ? -1 : it->second;
    }
    float get (const juce::String& id, float fallback = 0.0f) const
    {
        const int i = find (id);
        return i >= 0 ? values[(size_t) i] : fallback;
    }
    void rebuildIndex();
};

//==============================================================================
/**
    The editable scene model shared by UI, audio thread (MIDI learn) and render thread.
    All structure edits happen on the message thread; readers take a snapshot.
*/
class Scene
{
public:
    Scene();

    // ---- snapshotting (render thread)
    /** Copies structure if changed since `knownVersion`, values always. Returns current version. */
    uint32_t snapshot (SceneData& dest, uint32_t knownVersion) const;
    /** The render thread publishes final (modulated) values here for the UI to display. */
    void publishModulated (const std::vector<float>& finalValues, uint32_t version);
    float getModulated (const juce::String& id) const;

    // ---- params
    float getValue (const juce::String& id) const;
    void  setValue (const juce::String& id, float v);
    void  setNormalised (const juce::String& id, float n01);
    float getNormalised (const juce::String& id) const;
    bool  getParam (const juce::String& id, Param& out) const;
    std::vector<Param> getAllParams() const;

    // ---- layers
    juce::String addLayer (SourceType type, int sourceIndex = 0);
    void removeLayer (const juce::String& uid);
    void moveLayer (const juce::String& uid, int delta);
    void duplicateLayer (const juce::String& uid);
    bool getLayer (const juce::String& uid, Layer& out) const;
    std::vector<Layer> getLayers() const;
    template <typename Fn> void editLayer (const juce::String& uid, Fn&& fn)
    {
        const juce::ScopedLock sl (lock);
        if (auto* l = findLayer (uid))
        {
            const auto oldType = l->source; const auto oldIndex = l->sourceIndex;
            fn (*l);
            if (l->source != oldType || l->sourceIndex != oldIndex)
                rebuildSourceParams (*l);
            bump();
        }
    }
    /** Sets the type + index and re-creates source params. */
    void setLayerSource (const juce::String& uid, SourceType type, int index);

    // ---- effects (layerUid == "master" for master chain)
    juce::String addEffect (const juce::String& layerUid, const juce::String& effectId);
    void removeEffect (const juce::String& layerUid, const juce::String& fxUid);
    void moveEffect (const juce::String& layerUid, const juce::String& fxUid, int delta);
    void setEffectEnabled (const juce::String& layerUid, const juce::String& fxUid, bool);
    std::vector<EffectSlot> getEffects (const juce::String& layerUid) const;

    // ---- modulation
    juce::String addRoute (const ModRoute& r, float amount);
    void removeRoute (const juce::String& uid);
    void updateRoute (const ModRoute& r);
    std::vector<ModRoute> getRoutes() const;
    std::vector<ModRoute> getRoutesTo (const juce::String& dest) const;

    // ---- decks
    DeckState getDeck (int deck) const;
    void setDeck (int deck, const DeckState&);

    // ---- persistence
    juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree&);
    void resetToDefault();

    uint32_t getVersion() const { return version.load(); }

    // global param ids
    static constexpr const char* crossfaderId = "G.xfade";
    static constexpr const char* masterId     = "G.master";

private:
    void bump() { version.fetch_add (1); }
    Layer* findLayer (const juce::String& uid);
    std::vector<EffectSlot>* findChain (const juce::String& layerUid);
    juce::String nextUid (const char* prefix);
    void addParam (const juce::String& id, const juce::String& name, const juce::String& group,
                   float mn, float mx, float def);
    void removeParamsWithPrefix (const juce::String& prefix);
    void rebuildSourceParams (Layer&);
    void addEffectParams (const juce::String& layerUid, const EffectSlot&);
    void addGlobalParams();
    juce::String layerGroupName (const Layer&) const;

    mutable juce::CriticalSection lock;
    SceneData data;
    std::vector<float> modulated;
    uint32_t modulatedVersion = 0;
    std::atomic<uint32_t> version { 1 };
    int uidCounter = 1;
};

/** Param definitions a source of the given type exposes as p0..p7. */
std::vector<ParamDef> sourceParamDefs (SourceType, int sourceIndex);

constexpr int numVizTypes = 12;
juce::String vizTypeName (int);
} // namespace moshdeck
