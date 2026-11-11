#pragma once
#include <juce_core/juce_core.h>

namespace moshdeck
{
/** Audio-reactive visualizer "elements" (spectrum bars, circle spectrum, waveform,
    particles, pulsing image...). Stack several AudioViz layers to build a full
    music-visualizer template. Each returns `vec4 gen(vec2 uv)` with alpha coverage. */
struct VizLibrary
{
    static juce::String glsl (int type);
};
} // namespace moshdeck
