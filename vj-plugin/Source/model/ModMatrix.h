#pragma once
#include "Scene.h"
#include "audio/AudioAnalyzer.h"
#include <array>

namespace moshdeck
{
/** Everything a modulation source can read, gathered once per video frame. */
struct ModContext
{
    AudioFeatures audio;
    double beats = 0.0;     // musical position in quarter notes
    double bpm = 120.0;
    int beatsPerBar = 4;
    float beatPulse = 0.0f; // 1 at each beat, decays
    std::array<float, 16 * 128> cc {};   // 0..1
    std::array<float, 128> notes {};     // velocity 0..1 while held
    std::array<float, 16> macros {};     // 0..1
};

/**
    Evaluates the modulation matrix: final = base + sum(amount * source * range).
    "Param" sources read the previous frame's final value of another parameter,
    which allows arbitrary parameter linking (including feedback loops) without cycles.
*/
class ModEvaluator
{
public:
    void evaluate (const SceneData& scene, uint32_t sceneVersion, const ModContext& ctx, std::vector<float>& finalValues);

    static float lfo (int shape, double phase, double cycleIndex);
    float sourceValue (const ModRoute& r, const SceneData& scene, const ModContext& ctx) const;

private:
    std::vector<float> prevFinal;
    std::vector<float> smoothState; // per route
    uint32_t knownVersion = 0;
};
} // namespace moshdeck
