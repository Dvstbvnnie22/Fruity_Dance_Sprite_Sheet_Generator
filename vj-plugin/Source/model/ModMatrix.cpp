#include "ModMatrix.h"

namespace moshdeck
{
static float hashStep (double i)
{
    auto x = (uint32_t) (int64_t) std::floor (i);
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return (float) (x & 0xffffff) / (float) 0xffffff;
}

float ModEvaluator::lfo (int shape, double phase, double cycleIndex)
{
    const double p = phase - std::floor (phase);
    switch (shape)
    {
        case 0:  return (float) (0.5 - 0.5 * std::cos (p * juce::MathConstants<double>::twoPi));
        case 1:  return (float) (p < 0.5 ? p * 2.0 : 2.0 - p * 2.0);
        case 2:  return (float) p;
        case 3:  return p < 0.5 ? 1.0f : 0.0f;
        default: return hashStep (cycleIndex * 7.0 + 3.0);
    }
}

float ModEvaluator::sourceValue (const ModRoute& r, const SceneData& scene, const ModContext& ctx) const
{
    const auto& a = ctx.audio;
    switch (r.source)
    {
        case ModSourceType::AudioBand:  return a.bands[(size_t) juce::jlimit (0, 4, r.sourceIndex)];
        case ModSourceType::AudioLevel: return a.level;
        case ModSourceType::Kick:       return a.kick;
        case ModSourceType::Snare:      return a.snare;
        case ModSourceType::Hat:        return a.hat;
        case ModSourceType::BeatRamp:   return (float) (ctx.beats - std::floor (ctx.beats));
        case ModSourceType::BarRamp:
        {
            const double bars = ctx.beats / juce::jmax (1, ctx.beatsPerBar);
            return (float) (bars - std::floor (bars));
        }
        case ModSourceType::BeatPulse:  return ctx.beatPulse;
        case ModSourceType::LFO:
        {
            const juce::String n (juce::jlimit (0, 3, r.sourceIndex) + 1);
            const double div = juce::jmax (0.0625f, scene.get ("G.lfo" + n + ".div", 1.0f));
            const int shape = (int) std::round (scene.get ("G.lfo" + n + ".shape", 0.0f));
            const double ph = ctx.beats / div;
            return lfo (shape, ph, std::floor (ph));
        }
        case ModSourceType::RandomStep: return hashStep (std::floor (ctx.beats) * 13.0 + r.uid.hashCode() % 1000);
        case ModSourceType::MidiCC:     return ctx.cc[(size_t) juce::jlimit (0, 16 * 128 - 1, r.sourceIndex)];
        case ModSourceType::MidiNote:   return ctx.notes[(size_t) juce::jlimit (0, 127, r.sourceIndex)];
        case ModSourceType::Macro:      return ctx.macros[(size_t) juce::jlimit (0, 15, r.sourceIndex)];
        case ModSourceType::Param:
        {
            const int i = scene.find (r.sourceParam);
            if (i < 0) return 0.0f;
            const auto& p = scene.params[(size_t) i];
            const float v = (size_t) i < prevFinal.size() ? prevFinal[(size_t) i] : scene.values[(size_t) i];
            return p.max > p.min ? (v - p.min) / (p.max - p.min) : 0.0f;
        }
        default: break;
    }
    return 0.0f;
}

void ModEvaluator::evaluate (const SceneData& scene, uint32_t sceneVersion, const ModContext& ctx, std::vector<float>& out)
{
    if (sceneVersion != knownVersion || prevFinal.size() != scene.values.size())
    {
        prevFinal = scene.values;
        smoothState.assign (scene.routes.size(), 0.0f);
        knownVersion = sceneVersion;
    }
    out = scene.values;

    for (size_t ri = 0; ri < scene.routes.size(); ++ri)
    {
        const auto& r = scene.routes[ri];
        const int di = scene.find (r.dest);
        if (di < 0) continue;
        const int ai = scene.find (r.uid + ".amt"); // read the (already modulated) amount -> routes can modulate routes
        const float amount = ai >= 0 ? out[(size_t) ai] : 0.0f;
        if (amount == 0.0f) continue;

        float s = sourceValue (r, scene, ctx);
        auto& st = smoothState[ri];
        const float k = juce::jlimit (0.0f, 0.99f, r.smoothing);
        st = st * k + s * (1.0f - k);
        s = st;
        if (r.bipolar) s = s * 2.0f - 1.0f;

        const auto& p = scene.params[(size_t) di];
        out[(size_t) di] += amount * s * (p.max - p.min);
    }
    for (size_t i = 0; i < out.size(); ++i)
        out[i] = juce::jlimit (scene.params[i].min, scene.params[i].max, out[i]);
    prevFinal = out;
}
} // namespace moshdeck
