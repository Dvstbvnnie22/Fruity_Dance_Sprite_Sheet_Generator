#pragma once
#include <juce_core/juce_core.h>

namespace moshdeck::xf
{
constexpr int modeFade = 0, modeCut = 1;
constexpr int curveLinear = 0, curveSmooth = 1, curveSharpA = 2, curveSharpB = 3, numCurves = 4;

juce::String modeName (int mode);
juce::String curveName (int curve);

/** position: 0 = deck A ... 1 = deck B. Returns the deck B weight (0..1) that drives the video blend. */
float shape (float position, int mode, int curve, bool reverse, float cutPoint);

/** One-pole glide from current towards target over smoothMs (0 = instant). */
float smooth (float current, float target, float dtSeconds, float smoothMs);
}
