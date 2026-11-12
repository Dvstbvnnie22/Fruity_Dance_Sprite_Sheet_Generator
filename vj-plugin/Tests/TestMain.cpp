// Headless tests: expression evaluator, .milk parsing / per-frame execution,
// scene model + mod matrix, Shadertoy JSON import, and dumping every shader to
// disk so they can be validated with glslangValidator (see tools/validate_shaders.sh).
#include <juce_core/juce_core.h>
#include "milk/ExprEval.h"
#include "milk/MilkPreset.h"
#include "model/Scene.h"
#include "model/ModMatrix.h"
#include "render/EffectLibrary.h"
#include "render/ShadertoyImporter.h"
#include "render/ShaderPrelude.h"
#include "render/VizLibrary.h"
#include "model/Crossfader.h"

using namespace moshdeck;

static int failures = 0;
#define CHECK(cond) do { if (! (cond)) { ++failures; std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; } } while (0)
#define CHECK_NEAR(a, b) CHECK (std::abs ((double) (a) - (double) (b)) < 1e-6)

static double run (const char* src, const char* var)
{
    milk::VarTable v;
    milk::Program p;
    juce::String err;
    if (! p.compile (src, v, &err)) { std::cerr << "compile error: " << err << " in: " << src << "\n"; ++failures; return -999; }
    p.run (v);
    return v.get (var);
}

static void testExpr()
{
    CHECK_NEAR (run ("a = 1 + 2 * 3;", "a"), 7);
    CHECK_NEAR (run ("a = (1 + 2) * 3;", "a"), 9);
    CHECK_NEAR (run ("a = 2; a += 3; a *= 2;", "a"), 10);
    CHECK_NEAR (run ("a = above(3, 2) + below(3, 2) + equal(2, 2);", "a"), 2);
    CHECK_NEAR (run ("a = if(above(1, 0), 5, 6);", "a"), 5);
    CHECK_NEAR (run ("a = 7 % 3;", "a"), 1);
    CHECK_NEAR (run ("a = 5 | 2;", "a"), 7);
    CHECK_NEAR (run ("a = 6 & 3;", "a"), 2);
    CHECK_NEAR (run ("a = 3 > 2 ? 10 : 20;", "a"), 10);
    CHECK_NEAR (run ("a = -2^2;", "a"), -4);
    CHECK_NEAR (run ("a = sqr(3) + sqrt(16) + abs(-1);", "a"), 14);
    CHECK_NEAR (run ("a = min(2, 3) + max(2, 3);", "a"), 5);
    CHECK_NEAR (run ("a = 1/0;", "a"), 0);
    CHECK_NEAR (run ("megabuf(10) = 4; a = megabuf(10);", "a"), 4);
    CHECK_NEAR (run ("n = 0; loop(5, n = n + 2); a = n;", "a"), 10);
    CHECK_NEAR (run ("a = 1; // comment\n b = 2; /* block */ a = a + b;", "a"), 3);
    CHECK_NEAR (run ("A = 4; a = a + 1;", "a"), 5); // case insensitive
    CHECK_NEAR (run ("a = 1.5e1;", "a"), 15);
    CHECK_NEAR (run ("a = $PI > 3.14;", "a"), 1);
    CHECK_NEAR (run ("a = exec2(b = 3, b * 2);", "a"), 6);
    CHECK_NEAR (run ("a = 1 a = 2;", "a"), 2); // lenient missing ';'
}

static void testMilk()
{
    for (auto& name : milk::BuiltinMilkPresets::names())
    {
        auto d = milk::MilkPresetData::parse (milk::BuiltinMilkPresets::text (name), name);
        milk::MilkState s;
        juce::String errs;
        const bool ok = s.load (d, &errs);
        if (! ok) std::cerr << "preset " << name << " errors: " << errs << "\n";
        CHECK (ok);
        float wave[512], spec[512];
        for (int i = 0; i < 512; ++i) { wave[i] = std::sin (i * 0.1f) * 0.5f; spec[i] = 0.5f; }
        milk::MilkState::FrameInputs in;
        in.aspectX = 0.5625f;
        for (int f = 0; f < 10; ++f) { in.time = f / 60.0; in.frame = f; in.bass = 1.3f; s.runFrame (in, wave, spec); }
        bool finite = true;
        for (float u : s.meshUV()) finite = finite && std::isfinite (u);
        CHECK (finite);
    }
    auto d = milk::MilkPresetData::parse ("[preset00]\nfDecay=0.9\nzoom=1.1\nper_frame_1=rot = rot + 0.1;\nper_frame_2=q1 = 5;\nper_pixel_1=zoom = zoom + rad*0.0;\nshapecode_0_enabled=1\nshape_0_per_frame1=x = q1 * 0.1;\nwarp_1=`shader_body{}\n");
    CHECK_NEAR (d.base["decay"], 0.9);
    CHECK (d.hasPixelShaders);
    CHECK (d.shapes[0].enabled);
    milk::MilkState s;
    s.load (d);
    float w[512] = {}, sp[512] = {};
    milk::MilkState::FrameInputs in;
    s.runFrame (in, w, sp);
    CHECK_NEAR (s.get ("rot"), 0.1);
    s.runFrame (in, w, sp);
    CHECK_NEAR (s.get ("rot"), 0.1); // per-frame vars reset each frame
    CHECK (s.shapes[0].inst.size() == 1);
    CHECK (std::abs (s.shapes[0].inst[0].x - 0.5f) < 1e-5f);
}

static void testScene()
{
    Scene sc;
    const auto l = sc.addLayer (SourceType::Generator, 0);
    const auto fx = sc.addEffect (l, "mosh_bloom");
    CHECK (fx.isNotEmpty());
    Param p;
    CHECK (sc.getParam (l + "." + fx + ".p0", p));
    ModRoute r; r.source = ModSourceType::AudioBand; r.sourceIndex = 1; r.dest = l + ".opacity";
    const auto ru = sc.addRoute (r, 0.5f);
    sc.setValue (l + ".opacity", 0.2f);

    SceneData sd;
    const auto ver = sc.snapshot (sd, 0);
    ModContext ctx;
    ctx.audio.bands[1] = 1.0f;
    ModEvaluator ev;
    std::vector<float> out;
    ev.evaluate (sd, ver, ctx, out);
    CHECK (std::abs (out[(size_t) sd.find (l + ".opacity")] - 0.7f) < 1e-5f);

    // param link: speed follows opacity
    ModRoute link; link.source = ModSourceType::Param; link.sourceParam = l + ".opacity"; link.dest = l + ".speed";
    sc.addRoute (link, 0.25f);
    const auto ver2 = sc.snapshot (sd, ver);
    ev.evaluate (sd, ver2, ctx, out);
    ev.evaluate (sd, ver2, ctx, out);
    CHECK (out[(size_t) sd.find (l + ".speed")] > 1.0f);

    // round trip
    auto tree = sc.toValueTree();
    Scene sc2;
    sc2.fromValueTree (tree);
    CHECK (sc2.getLayers().size() == sc.getLayers().size());
    CHECK (std::abs (sc2.getValue (l + ".opacity") - 0.2f) < 1e-5f);
    CHECK (sc2.getRoutes().size() == 2);
    sc.removeLayer (l);
    CHECK (sc.getRoutes().empty());
    juce::ignoreUnused (ru);
}

static void testShadertoy()
{
    juce::String err;
    auto p = ShadertoyProject::parse (ShadertoyProject::defaultShader(), err);
    CHECK (err.isEmpty());
    CHECK (p.isValid());
    const char* json = R"JSON({"Shader":{"info":{"name":"Test"},"renderpass":[
      {"type":"common","code":"float k(){return 1.0;}","inputs":[],"outputs":[]},
      {"type":"buffer","name":"Buffer A","code":"void mainImage(out vec4 c, in vec2 f){ c = texture(iChannel0, f/iResolution.xy)*0.9 + k()*0.01; }",
       "inputs":[{"id":"4dXGR8","channel":0,"ctype":"buffer","sampler":{"filter":"linear","wrap":"clamp"}}],"outputs":[{"id":"4dXGR8","channel":0}]},
      {"type":"image","name":"Image","code":"void mainImage(out vec4 c, in vec2 f){ c = texture(iChannel0, f/iResolution.xy) + texture(iChannel1, vec2(0.1,0.25)).x; }",
       "inputs":[{"id":"4dXGR8","channel":0,"ctype":"buffer"},{"id":"x","channel":1,"ctype":"music"}],"outputs":[]}]}})JSON";
    auto q = ShadertoyProject::parse (json, err);
    CHECK (err.isEmpty());
    CHECK (q.passes.size() == 2);
    CHECK (q.passes[0].output == 0);
    CHECK (q.passes[0].channels[0].kind == ShadertoyProject::ChannelKind::Self);
    CHECK (q.passes[1].channels[0].kind == ShadertoyProject::ChannelKind::Buffer);
    CHECK (q.passes[1].channels[1].kind == ShadertoyProject::ChannelKind::Audio);
}

static void testCrossfader()
{
    using namespace xf;
    for (int c = 0; c < numCurves; ++c)
    {
        CHECK_NEAR (shape (0.0f, modeFade, c, false, 0.5f), 0.0);
        CHECK_NEAR (shape (1.0f, modeFade, c, false, 0.5f), 1.0);
        float prev = -1.0f;
        for (int i = 0; i <= 200; ++i)
        {
            const float v = shape (i / 200.0f, modeFade, c, false, 0.5f);
            CHECK (v >= prev - 1e-6f);   // never goes backwards
            CHECK (v >= 0.0f && v <= 1.0f);
            prev = v;
        }
    }
    // curves differ where they should
    CHECK (shape (0.25f, modeFade, curveSharpA, false, 0.5f) < shape (0.25f, modeFade, curveLinear, false, 0.5f));
    CHECK (shape (0.25f, modeFade, curveSharpB, false, 0.5f) > shape (0.25f, modeFade, curveLinear, false, 0.5f));
    CHECK (shape (0.5f, modeFade, curveSmooth, false, 0.5f) > 0.49f && shape (0.5f, modeFade, curveSmooth, false, 0.5f) < 0.51f);

    // reverse (hamster / inverse) mirrors the position
    CHECK_NEAR (shape (0.2f, modeFade, curveLinear, true, 0.5f), shape (0.8f, modeFade, curveLinear, false, 0.5f));
    CHECK_NEAR (shape (0.0f, modeFade, curveSharpB, true, 0.5f), 1.0);

    // cut mode: hard switch at the cut point
    CHECK_NEAR (shape (0.3f, modeCut, curveLinear, false, 0.5f), 0.0);
    CHECK_NEAR (shape (0.7f, modeCut, curveLinear, false, 0.5f), 1.0);
    CHECK_NEAR (shape (0.7f, modeCut, curveLinear, false, 0.8f), 0.0);  // cut moved right: B is later
    CHECK_NEAR (shape (0.3f, modeCut, curveLinear, true, 0.5f), 1.0);   // reversed: now B on the left

    // smoothing: instant when 0, settles within about a second at 200 ms
    CHECK_NEAR (smooth (0.0f, 1.0f, 1.0f / 60.0f, 0.0f), 1.0);
    float v = 0.0f;
    for (int i = 0; i < 60; ++i) v = smooth (v, 1.0f, 1.0f / 60.0f, 200.0f);
    CHECK (v > 0.98f);

    // the parameters exist in the scene and default to a plain fade
    Scene sc;
    Param prm;
    CHECK (sc.getParam ("G.xfade.mode", prm) && prm.def == 0.0f);
    CHECK (sc.getParam ("G.xfade.curve", prm) && prm.max == 3.0f);
    CHECK (sc.getParam ("G.xfade.rev", prm));
    CHECK (sc.getParam ("G.xfade.cut", prm) && prm.def == 0.5f);
    CHECK (sc.getParam ("G.xfade.smooth", prm));
}

static void dumpShaders (const juce::File& dir)
{
    dir.createDirectory();
    for (auto& e : EffectLibrary::all())
        dir.getChildFile ("fx_" + e.id + ".frag").replaceWithText (juce::String (shaders::prelude) + e.glsl + "\nvoid main(){ fragColor = fx(vUv); }\n");
    for (auto& g : GeneratorLibrary::all())
        dir.getChildFile ("gen_" + g.id + ".frag").replaceWithText (juce::String (shaders::prelude) + g.glsl + "\nvoid main(){ fragColor = gen(vUv); }\n");
    for (int i = 0; i < numVizTypes; ++i)
        dir.getChildFile ("viz_" + juce::String (i) + ".frag").replaceWithText (juce::String (shaders::prelude) + VizLibrary::glsl (i) + "\nvoid main(){ fragColor = gen(vUv); }\n");
    juce::String err;
    auto p = ShadertoyProject::parse (ShadertoyProject::defaultShader(), err);
    dir.getChildFile ("shadertoy_default.frag").replaceWithText (p.buildFragmentSource (p.passes[0]));
}

int main (int argc, char** argv)
{
    testExpr();
    testMilk();
    testScene();
    testShadertoy();
    testCrossfader();
    if (argc > 1) dumpShaders (juce::File (argv[1]));
    std::cout << "effects: " << EffectLibrary::all().size() << ", generators: " << GeneratorLibrary::count()
              << ", milk presets: " << milk::BuiltinMilkPresets::names().size() << "\n";
    std::cout << (failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED") << " (" << failures << " failures)\n";
    return failures == 0 ? 0 : 1;
}
