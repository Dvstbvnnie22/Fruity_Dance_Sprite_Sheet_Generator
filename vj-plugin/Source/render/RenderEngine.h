#pragma once
#include "GLUtils.h"
#include "MilkRenderer.h"
#include "ShadertoyImporter.h"
#include "model/ModMatrix.h"
#include "media/MediaDeck.h"
#include <map>
#include <memory>

namespace moshdeck
{
class Scene;

/** Per-frame information handed to the engine by the plugin. */
struct FrameInput
{
    ModContext mod;
    bool hostPlaying = false;
};

/** Implemented by the processor; gives the engine everything that lives outside the GL context. */
struct EngineHost
{
    virtual ~EngineHost() = default;
    virtual Scene& getScene() = 0;
    virtual MediaDeck& getDeck (int index) = 0;
    virtual void buildFrameInput (FrameInput&) = 0;
    virtual void getCanvasSize (int& w, int& h) = 0;
    virtual void setLayerStatus (const juce::String& layerUid, const juce::String& status) = 0;
    virtual void onDeckAdvance (int deck) = 0;   // playlist "next clip" timing reached
    virtual void onMilkAdvance (const juce::String& layerUid) = 0;
};

/**
    The visual engine: renders the layer stack (sources -> effect chains -> blend)
    into an offscreen canvas and presents it. Must be used on a thread with a
    current OpenGL 3.2+ core context (JUCE OpenGLRenderer callbacks).
*/
class RenderEngine
{
public:
    explicit RenderEngine (EngineHost& host);
    ~RenderEngine();

    void contextCreated();
    void renderFrame (int viewportW, int viewportH, float scale);
    void contextClosing();

    /** Optionally capture a small RGB thumbnail of the output (for a second preview). */
    void requestThumbnail (bool on) { wantThumb = on; }
    bool getThumbnail (juce::Image& dest);

    double getFps() const { return fps.load(); }
    /** Compile errors of built-in shaders (GL thread only). */
    const std::map<juce::String, juce::String>& getShaderErrors() const { return shaderErrors; }

private:
    struct FxRuntime
    {
        gl::Target out[2], prevIn;
        int cur = 0, localFrame = 0;
    };
    struct ShadertoyRuntime
    {
        juce::String sourceHash;
        ShadertoyProject project;
        std::vector<std::unique_ptr<gl::Shader>> shaders;
        gl::Target buffers[4][2];
        int latest[4] = { 0, 0, 0, 0 };
        int frame = 0;
        bool ok = false;
    };
    struct LayerRuntime
    {
        gl::Target src, chain[2], genPrev[2];
        int genCur = 0;
        double time = 0.0;
        std::map<juce::String, FxRuntime> fx;
        std::unique_ptr<MilkSource> milk;
        std::unique_ptr<ShadertoyRuntime> st;
        double lastAutoBeat = -1.0;
        int localFrame = 0;
    };

    struct FrameState
    {
        float time = 0, beats = 0, beat = 0, bar = 0, bpm = 120;
        float bands[5] {};
        float level = 0, kick = 0, snare = 0, hat = 0, pulse = 0, rnd = 0;
        float trigBeat = 0, trigBar = 0, trigKick = 0;
        int frame = 0;
        int W = 1280, H = 720;
    };

    gl::Shader* effectShader (const juce::String& id);
    gl::Shader* generatorShader (int index);
    gl::Shader* vizShader (int type);
    gl::Shader* builtin (const juce::String& key, const juce::String& fsBody);
    void setCommon (gl::Shader& s, const float* p8, float mix, int localFrame);
    void bindCommonTextures (GLuint in, GLuint prev, GLuint prevIn, GLuint below);
    GLuint inputFor (InputRoute r, GLuint below) const;
    void updateDecks (double dt, const SceneData& sd, const std::vector<float>& vals);
    void renderSource (LayerRuntime& rt, const Layer& l, const std::vector<float>& vals, GLuint input, GLuint below, double dt);
    GLuint runChain (const juce::String& owner, std::map<juce::String, FxRuntime>& fxs, const std::vector<EffectSlot>& chain,
                     gl::Target* pingPong, GLuint input, gl::Target* inputTarget, const std::vector<float>& vals, GLuint below);
    void renderShadertoy (LayerRuntime& rt, const Layer& l, const std::vector<float>& vals, GLuint input, GLuint below, gl::Target& dest);
    void blit (const gl::Target& src, gl::Target& dst);
    float val (const juce::String& id, float fallback = 0.0f) const;

    EngineHost& host;
    gl::Geometry geo;
    MilkShaders milkShaders;
    std::map<juce::String, std::unique_ptr<gl::Shader>> shaders;
    std::map<juce::String, juce::String> shaderErrors;
    std::map<juce::String, std::unique_ptr<LayerRuntime>> layers;
    std::map<juce::String, FxRuntime> masterFx;
    gl::Target comp[2], masterChain[2], output, deckFit[2][2], deckMix, thumbTarget;
    int deckFitCur = 0;
    gl::Texture deckTex[2], audioTex, specTex, blackTex, noiseTex;
    VideoFrame deckFrames[2];
    uint64_t deckSerial[2] = { 0, 0 };
    bool deckHasFrame[2] = { false, false };

    SceneData scene;
    uint32_t sceneVersion = 0;
    ModEvaluator modEval;
    std::vector<float> finalValues;
    FrameInput frameIn;
    FrameState fs;
    double lastTime = 0.0, startTime = 0.0;
    double prevBeats = 0.0;
    uint32_t prevKickCount = 0;
    float pulse = 0.0f;
    int frameCounter = 0;
    double lastDeckBar[2] = { -1.0, -1.0 };
    float xfShown = 0.0f;
    bool xfInit = false;

    std::atomic<double> fps { 0.0 };
    bool wantThumb = false;
    juce::SpinLock thumbLock;
    juce::Image thumb;
    std::vector<uint8_t> thumbPixels;
    bool ready = false;
};
} // namespace moshdeck
