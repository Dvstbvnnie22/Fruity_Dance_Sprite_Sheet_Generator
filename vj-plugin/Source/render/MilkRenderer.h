#pragma once
#include "GLUtils.h"
#include "milk/MilkPreset.h"
#include <memory>

namespace moshdeck
{
/** Shared GL programs for every Milk instance (one set per GL context). */
class MilkShaders
{
public:
    bool create (juce::String& log);
    void release();
    gl::Shader warp, draw, comp, copy;
};

struct MilkFrameParams
{
    milk::MilkState::FrameInputs in;
    const float* wave512 = nullptr;
    const float* spectrum512 = nullptr;
    GLuint inputTex = 0;     // base picture injected into the feedback buffer
    float inject = 0.0f;
    int injectMode = 0;      // 0 const, 1 beat, 2 kick, 3 luma
    float beatPulse = 0.0f, kick = 0.0f;
    int width = 1280, height = 720;
};

/** One running MilkDrop-format preset: CPU equations + GL feedback buffers. */
class MilkInstance
{
public:
    bool load (const milk::MilkPresetData& d, juce::String& errors);
    void render (MilkShaders& sh, gl::Geometry& geo, const MilkFrameParams& p);
    void seedFrom (MilkShaders& sh, gl::Geometry& geo, const MilkInstance& other);
    GLuint output() const { return out.tex; }
    juce::String name() const { return state.name; }

private:
    void drawWave (gl::Geometry& geo, const milk::MilkState::WaveOut& w, int W, int H);
    void drawShapes (MilkShaders& sh, gl::Geometry& geo, int W, int H, GLuint prevTex);
    void drawBorders (gl::Geometry& geo);
    void drawMotionVectors (gl::Geometry& geo, int W, int H);

    milk::MilkState state;
    gl::Target buf[2], out;
    int cur = 0;
    std::vector<float> verts;
};

/** A layer source that plays Milk presets with smooth blending between them. */
class MilkSource
{
public:
    /** spec: "builtin:<name>" or a .milk file path. */
    void setPreset (const juce::String& spec, float blendSeconds = 2.0f);
    void render (MilkShaders& sh, gl::Geometry& geo, const MilkFrameParams& p, gl::Target& dest);
    juce::String currentSpec() const { return spec; }
    juce::String lastErrors;
    bool hasPixelShaders = false;

private:
    std::unique_ptr<MilkInstance> cur, prev;
    juce::String spec, pendingSpec;
    double blendStart = -1.0, blendTime = 2.0, now = 0.0;
};
} // namespace moshdeck
