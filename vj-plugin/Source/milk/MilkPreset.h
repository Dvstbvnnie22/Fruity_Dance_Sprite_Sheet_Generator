#pragma once
#include "ExprEval.h"
#include <array>
#include <map>

namespace moshdeck::milk
{
/** Parsed contents of a MilkDrop-format .milk preset file. */
struct MilkPresetData
{
    juce::String name;
    std::map<std::string, double> base;   // engine variable name -> value
    std::string perFrameInit, perFrame, perPixel;

    struct Shape
    {
        std::map<std::string, double> base;
        std::string init, perFrame;
        bool enabled = false;
    };
    struct Wave
    {
        std::map<std::string, double> base;
        std::string init, perFrame, perPoint;
        bool enabled = false;
    };
    std::array<Shape, 4> shapes;
    std::array<Wave, 4> waves;
    bool hasPixelShaders = false; // warp_/comp_ HLSL present (not executed by this engine)

    static MilkPresetData parse (const juce::String& text, const juce::String& name = {});
    static MilkPresetData load (const juce::File& f);
};

/** Maps .milk file keys (fDecay, nWaveMode...) to the per-frame variable names (decay, wave_mode...). */
std::string milkKeyToVar (const juce::String& key);

/** Built-in, original presets in .milk format. */
struct BuiltinMilkPresets
{
    static const juce::StringArray& names();
    static juce::String text (const juce::String& name);
};

/**
    CPU side of a running preset: variables, compiled equations and the
    per-vertex warp mesh. The GL renderer (MilkRenderer) draws from this.
*/
class MilkState
{
public:
    static constexpr int meshW = 48, meshH = 36;

    struct FrameInputs
    {
        double time = 0, fps = 60, dt = 1.0 / 60.0;
        int frame = 0;
        float bass = 0, mid = 0, treb = 0, bassAtt = 0, midAtt = 0, trebAtt = 0;
        float aspectX = 1, aspectY = 1;
        int pixelsX = 1280, pixelsY = 720;
        // user offsets from the layer params
        float zoomOffset = 0, rotOffset = 0, warpScale = 1, decayOffset = 0, waveScale = 1;
    };

    struct ShapeOut
    {
        bool enabled = false;
        int instances = 1;
        struct Inst { float x, y, rad, ang, r, g, b, a, r2, g2, b2, a2, br, bg, bb, ba; int sides; bool additive, thick, textured; float texZoom, texAng; };
        std::vector<Inst> inst;
    };

    struct WaveOut
    {
        bool enabled = false, dots = false, thick = false, additive = false;
        std::vector<float> xy;    // interleaved x,y (0..1, y up)
        std::vector<float> rgba;  // per point
    };

    bool load (const MilkPresetData& data, juce::String* errors = nullptr);
    void runFrame (const FrameInputs& in, const float* wave512, const float* spectrum512);

    // per-frame results
    double get (const char* name) const { return vars.get (name); }
    const std::vector<float>& meshUV() const { return uv; }   // (meshW+1)*(meshH+1)*2, 0..1, y up
    std::array<ShapeOut, 4> shapes;
    std::array<WaveOut, 4> waves;
    WaveOut mainWave;
    juce::String name;
    bool hasPixelShaders = false;

private:
    void computeMesh (const FrameInputs& in);
    void buildMainWave (const FrameInputs& in, const float* wave512);

    VarTable vars;
    std::vector<double> gmegabuf;
    Program progInit, progFrame, progPixel;
    std::map<std::string, double> baseVals;
    std::array<double, 32> qInit {};

    struct ShapeProg { VarTable vars; Program init, frame; std::map<std::string, double> base; bool enabled = false; bool inited = false; };
    struct WaveProg  { VarTable vars; Program init, frame, point; std::map<std::string, double> base; bool enabled = false; bool inited = false; };
    std::array<ShapeProg, 4> shapeProgs;
    std::array<WaveProg, 4> waveProgs;

    // cached var slots
    int vX = 0, vY = 0, vRad = 0, vAng = 0, vZoom = 0, vZoomExp = 0, vRot = 0, vWarp = 0,
        vCx = 0, vCy = 0, vDx = 0, vDy = 0, vSx = 0, vSy = 0;
    std::vector<float> uv;
    bool initDone = false;
};
} // namespace moshdeck::milk
