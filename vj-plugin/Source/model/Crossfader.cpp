#include "Crossfader.h"
#include <cmath>

namespace moshdeck::xf
{
juce::String modeName (int mode)
{
    return mode == modeCut ? "Cut" : "Fade";
}

juce::String curveName (int curve)
{
    static const char* names[] = { "Linear", "Smooth", "Sharp A", "Sharp B" };
    return names[juce::jlimit (0, numCurves - 1, curve)];
}

float shape (float position, int mode, int curve, bool reverse, float cutPoint)
{
    float x = juce::jlimit (0.0f, 1.0f, position);
    if (reverse)
        x = 1.0f - x; // "hamster" / inverse switch: flips the direction

    if (mode == modeCut)
        return x > juce::jlimit (0.0f, 1.0f, cutPoint) ? 1.0f : 0.0f;

    switch (curve)
    {
        case curveSmooth: return x * x * (3.0f - 2.0f * x);   // S-curve: soft at both ends
        case curveSharpA: return std::pow (x, 2.5f);          // B only arrives near the far end
        case curveSharpB: return 1.0f - std::pow (1.0f - x, 2.5f); // B arrives almost immediately
        default: break;
    }
    return x; // linear
}

float smooth (float current, float target, float dt, float smoothMs)
{
    if (smoothMs <= 0.0f)
        return target;
    if (dt <= 0.0f)
        return current;
    const float tau = smoothMs * 0.001f;
    return current + (target - current) * (1.0f - std::exp (-dt / tau));
}
}
