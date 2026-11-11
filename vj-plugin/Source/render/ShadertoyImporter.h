#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <vector>

namespace moshdeck
{
/**
    Parses Shadertoy shaders - either raw GLSL (a `mainImage` function, optional
    `// @iChannelN: <kind>` directives) or the JSON export / API format with
    Common + Buffer A..D + Image passes - into a GL-independent description.
*/
struct ShadertoyProject
{
    enum class ChannelKind { None, Input, DeckA, DeckB, Below, Audio, Buffer, Self, Noise };

    struct Channel
    {
        ChannelKind kind = ChannelKind::None;
        int buffer = -1;          // 0..3 for Buffer
        bool linear = true;
        bool repeat = false;
    };

    struct Pass
    {
        juce::String name;        // "Image", "Buffer A"...
        int output = -1;          // -1 = image, 0..3 = buffer A..D
        juce::String code;
        std::array<Channel, 4> channels;
    };

    juce::String name;
    juce::String common;
    std::vector<Pass> passes;     // execution order: buffers A..D, then Image

    bool isValid() const { return ! passes.empty() && passes.back().output == -1; }

    static ShadertoyProject parse (const juce::String& text, juce::String& error);

    /** Full GLSL 330 fragment source for a pass (with the Shadertoy uniform block). */
    juce::String buildFragmentSource (const Pass& pass) const;

    /** Downloads a shader via the official API (requires a Shadertoy API key). Blocking. */
    static juce::String fetchFromWeb (const juce::String& shaderIdOrUrl, const juce::String& apiKey, juce::String& error);

    static juce::String defaultShader();
};
} // namespace moshdeck
