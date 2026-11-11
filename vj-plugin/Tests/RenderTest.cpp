// GPU smoke test: renders every generator, visualizer, Milk preset and effect
// through the real RenderEngine, reports shader errors / all-black output and
// writes contact sheets. Run under a display (e.g. xvfb-run).
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_opengl/juce_opengl.h>
#include "render/RenderEngine.h"
#include "render/EffectLibrary.h"
#include "milk/MilkPreset.h"
#include "model/Scene.h"
#include "audio/AudioAnalyzer.h"

using namespace moshdeck;

struct TestHost : EngineHost
{
    Scene scene;
    MediaDeck decks[2];
    AudioAnalyzer analyzer;
    double beats = 0.0;
    float macros[16] = {};
    std::map<juce::String, juce::String> status;
    Scene& getScene() override { return scene; }
    MediaDeck& getDeck (int i) override { return decks[i]; }
    void buildFrameInput (FrameInput& in) override
    {
        analyzer.read (in.mod.audio);
        in.mod.beats = beats; in.mod.bpm = 120.0; in.mod.beatsPerBar = 4;
        in.mod.beatPulse = (float) std::exp (-(beats - std::floor (beats)) * 6.0);
        for (int i = 0; i < 16; ++i) in.mod.macros[(size_t) i] = macros[i];
    }
    void getCanvasSize (int& w, int& h) override { w = 480; h = 270; }
    void setLayerStatus (const juce::String& uid, const juce::String& s) override { status[uid] = s; }
    void onDeckAdvance (int) override {}
    void onMilkAdvance (const juce::String&) override {}
};

struct Phase { juce::String name, sheet; std::function<void (Scene&)> setup; int frames; };

class TestComponent : public juce::Component, private juce::OpenGLRenderer
{
public:
    TestComponent (const juce::File& outDir, const juce::String& deckImage, const juce::String& rig) : out (outDir), engine (host), rigFile (rig)
    {
        host.analyzer.prepare (44100.0);
        if (deckImage.isNotEmpty()) host.decks[0].load (deckImage);
        host.decks[1].load (out.getChildFile ("pattern.png").getFullPathName());
        buildPhases();
        setSize (480, 270);
        ctx.setRenderer (this);
        ctx.setOpenGLVersionRequired (juce::OpenGLContext::openGL3_2);
        ctx.setComponentPaintingEnabled (false);
        ctx.setContinuousRepainting (true);
        ctx.attachTo (*this);
    }
    ~TestComponent() override { ctx.detach(); }

    std::atomic<bool> done { false };
    int failures = 0;

private:
    void buildPhases()
    {
        if (rigFile.isNotEmpty())
        {
            for (int k = 0; k < 3; ++k)
                phases.push_back ({ "rig:" + juce::String (k == 0 ? "intro" : k == 1 ? "build" : "drop"), "rig", [this, k] (Scene& s)
                {
                    if (k == 0)
                        if (auto xml = juce::XmlDocument::parse (juce::File (rigFile)))
                            s.fromValueTree (juce::ValueTree::fromXml (*xml).getChildWithName ("MoshDeckScene"));
                    for (int i = 0; i < 16; ++i) host.macros[i] = k == 0 ? 0.0f : (k == 1 ? 0.5f : 0.8f);
                    host.macros[10] = k == 2 ? 1.0f : 0.0f;   // Macro 11 = DROP
                    host.macros[11] = 0.0f;                  // strobe off for the screenshot
                    host.macros[9] = 1.0f;
                }, 150 });
            return;
        }
        auto clear = [] (Scene& s) { for (auto& l : s.getLayers()) s.removeLayer (l.uid); for (auto& f : s.getEffects ("master")) s.removeEffect ("master", f.uid); };
        for (int i = 0; i < GeneratorLibrary::count(); ++i)
            phases.push_back ({ "gen:" + GeneratorLibrary::get (i)->id, "generators", [=] (Scene& s)
            {
                clear (s);
                auto u = s.addLayer (SourceType::Generator, i);
                s.editLayer (u, [] (Layer& l) { l.input = InputRoute::DeckA; });
            }, 40 });
        for (int i = 0; i < numVizTypes; ++i)
            phases.push_back ({ "viz:" + vizTypeName (i), "visualizers", [=] (Scene& s)
            {
                clear (s);
                s.addLayer (SourceType::DeckB, 0);
                auto u = s.addLayer (SourceType::AudioViz, i);
                s.editLayer (u, [] (Layer& l) { l.input = InputRoute::DeckA; });
            }, 30 });
        for (auto& n : milk::BuiltinMilkPresets::names())
            phases.push_back ({ "milk:" + n, "milk", [=] (Scene& s)
            {
                clear (s);
                auto u = s.addLayer (SourceType::Milk, 0);
                s.editLayer (u, [=] (Layer& l) { l.milkPreset = "builtin:" + n; l.input = InputRoute::DeckA; });
                s.setValue (u + ".p0", 0.05f);
            }, 120 });
        for (auto& e : EffectLibrary::all())
        {
            const auto id = e.id;
            phases.push_back ({ "fx:" + id, "effects", [=] (Scene& s)
            {
                clear (s);
                auto u = s.addLayer (SourceType::DeckA, 0);
                s.setValue (u + ".p4", 0.3f); // bass pulse so there is motion for datamosh
                s.setValue (u + ".p3", 0.0f);
                s.addEffect (u, id);
                ModRoute r; r.source = ModSourceType::LFO; r.sourceIndex = 0; r.dest = u + ".p1"; r.bipolar = true;
                s.addRoute (r, 0.05f);
            }, 50 });
        }
        phases.push_back ({ "shadertoy:default", "misc", [=] (Scene& s)
        {
            clear (s);
            auto u = s.addLayer (SourceType::Shadertoy, 0);
            s.editLayer (u, [] (Layer& l) { l.input = InputRoute::DeckA; });
        }, 30 });
        phases.push_back ({ "shadertoy:multipass", "misc", [=] (Scene& s)
        {
            clear (s);
            auto u = s.addLayer (SourceType::Shadertoy, 0);
            const char* json = R"JSON({"Shader":{"info":{"name":"fb"},"renderpass":[
              {"type":"buffer","name":"Buffer A","code":"void mainImage(out vec4 c, in vec2 f){ vec2 uv=f/iResolution.xy; vec4 p=texture(iChannel0, (uv-0.5)*0.98+0.5); float d=length(uv-vec2(0.5+0.3*sin(iTime),0.5+0.3*cos(iTime*1.3))); c = p*0.97 + vec4(0.5+0.5*cos(iTime+vec3(0,2,4)),1)*smoothstep(0.05,0.0,d); }",
               "inputs":[{"id":"4dXGR8","channel":0,"ctype":"buffer"}],"outputs":[{"id":"4dXGR8","channel":0}]},
              {"type":"image","name":"Image","code":"void mainImage(out vec4 c, in vec2 f){ c = texture(iChannel0, f/iResolution.xy); }",
               "inputs":[{"id":"4dXGR8","channel":0,"ctype":"buffer"}],"outputs":[]}]}})JSON";
            s.editLayer (u, [=] (Layer& l) { l.shaderCode = json; });
        }, 60 });
        phases.push_back ({ "stack:full", "misc", [=] (Scene& s)
        {
            clear (s);
            s.addLayer (SourceType::DeckMix, 0);
            auto m = s.addLayer (SourceType::Milk, 0);
            s.editLayer (m, [] (Layer& l) { l.milkPreset = "builtin:Kaleid Bloom"; l.blendMode = (int) BlendMode::Screen; l.input = InputRoute::Below; });
            s.addEffect (m, "mosh_bloom");
            auto v = s.addLayer (SourceType::AudioViz, 2);
            s.editLayer (v, [] (Layer& l) { l.input = InputRoute::DeckA; });
            s.addEffect ("master", "rgb_split");
            s.addEffect ("master", "crt");
        }, 90 });
    }

    void newOpenGLContextCreated() override { engine.contextCreated(); }
    void openGLContextClosing() override { engine.contextClosing(); }

    void pushAudio()
    {
        // synthetic 120 BPM groove: kick on beats, snare on 2/4, hats on 8ths, a pad
        const int n = 735;
        std::vector<float> buf ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            const double t = sampleTime / 44100.0;
            const double beat = t * 2.0, ph = beat - std::floor (beat);
            const double eighth = beat * 2.0 - std::floor (beat * 2.0);
            const double kick = std::sin (2 * juce::MathConstants<double>::pi * (50 + 100 * std::exp (-ph * 30)) * ph * 0.5) * std::exp (-ph * 8);
            const bool sn = ((int) std::floor (beat)) % 2 == 1;
            const double snare = sn ? (rng.nextFloat() * 2 - 1) * std::exp (-ph * 18) * 0.5 : 0.0;
            const double hat = (rng.nextFloat() * 2 - 1) * std::exp (-eighth * 60) * 0.25;
            const double pad = 0.15 * (std::sin (t * 2 * juce::MathConstants<double>::pi * 220) + std::sin (t * 2 * juce::MathConstants<double>::pi * 277));
            buf[(size_t) i] = (float) (kick * 0.8 + snare + hat + pad);
            ++sampleTime;
        }
        const float* ch[1] = { buf.data() };
        host.analyzer.push (ch, 1, n);
        host.beats = sampleTime / 44100.0 * 2.0;
    }

    void renderOpenGL() override
    {
        using namespace juce::gl;
        if (done) return;
        if (phaseIndex >= (int) phases.size()) { finish(); return; }
        auto& ph = phases[(size_t) phaseIndex];
        if (frame == 0) { ph.setup (host.scene); std::cout << "phase " << ph.name << std::endl; }
        pushAudio();
        engine.renderFrame (480, 270, 1.0f);
        const GLenum err = glGetError();
        if (err != GL_NO_ERROR) { std::cout << "  GL error 0x" << juce::String::toHexString ((int) err) << " in " << ph.name << std::endl; ++failures; }
        ++frame;
        if (frame >= ph.frames)
        {
            std::vector<uint8_t> px (480 * 270 * 4);
            glReadPixels (0, 0, 480, 270, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
            juce::Image img (juce::Image::RGB, 480, 270, false);
            double sum = 0;
            for (int y = 0; y < 270; ++y)
                for (int x = 0; x < 480; ++x)
                {
                    auto* p = &px[(size_t) (((269 - y) * 480 + x) * 4)];
                    img.setPixelAt (x, y, juce::Colour (p[0], p[1], p[2]));
                    sum += p[0] + p[1] + p[2];
                }
            const double mean = sum / (480.0 * 270.0 * 3.0);
            if (mean < 1.0) { std::cout << "  WARNING: output is black (" << mean << ") in " << ph.name << std::endl; }
            shots[ph.sheet].push_back ({ ph.name, img });
            for (auto& kv : host.status)
                if (kv.second.containsIgnoreCase ("error")) { std::cout << "  status " << kv.second << std::endl; ++failures; }
            host.status.clear();
            frame = 0;
            ++phaseIndex;
        }
    }

    void finish()
    {
        for (auto& e : engine.getShaderErrors()) { std::cout << "SHADER ERROR " << e.first << ":\n" << e.second << std::endl; ++failures; }
        for (auto& kv : shots)
        {
            const int cols = 6, tw = 240, th = 135, labelH = 14;
            const int rows = ((int) kv.second.size() + cols - 1) / cols;
            juce::Image sheet (juce::Image::RGB, cols * tw, rows * (th + labelH), true);
            juce::Graphics g (sheet);
            g.fillAll (juce::Colours::black);
            for (size_t i = 0; i < kv.second.size(); ++i)
            {
                const int x = (int) (i % cols) * tw, y = (int) (i / cols) * (th + labelH);
                g.drawImage (kv.second[i].second, x, y, tw, th, 0, 0, 480, 270);
                g.setColour (juce::Colours::white);
                g.setFont (11.0f);
                g.drawText (kv.second[i].first, x + 2, y + th, tw - 4, labelH, juce::Justification::centredLeft);
            }
            juce::PNGImageFormat png;
            auto f = out.getChildFile ("sheet_" + kv.first + ".png");
            f.deleteFile();
            juce::FileOutputStream os (f);
            png.writeImageToStream (sheet, os);
        }
        std::cout << (failures == 0 ? "RENDER TEST PASSED" : "RENDER TEST FAILED") << " (" << failures << ")" << std::endl;
        done = true;
        juce::MessageManager::callAsync ([this] { juce::JUCEApplicationBase::getInstance()->setApplicationReturnValue (failures == 0 ? 0 : 1); juce::JUCEApplicationBase::quit(); });
    }

    juce::File out;
    TestHost host;
    juce::String rigFile;
    juce::OpenGLContext ctx;
    RenderEngine engine;
    std::vector<Phase> phases;
    int phaseIndex = 0, frame = 0;
    long long sampleTime = 0;
    juce::Random rng { 42 };
    std::map<juce::String, std::vector<std::pair<juce::String, juce::Image>>> shots;
};

class RenderTestApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "MoshDeckRenderTest"; }
    const juce::String getApplicationVersion() override { return "1.0"; }
    void initialise (const juce::String& cmd) override
    {
        auto args = juce::StringArray::fromTokens (cmd, true);
        const juce::File outDir (args.size() > 0 ? args[0].unquoted() : juce::File::getCurrentWorkingDirectory().getFullPathName());
        outDir.createDirectory();
        // deck B test pattern
        juce::Image pat (juce::Image::RGB, 640, 360, true);
        {
            juce::Graphics g (pat);
            for (int i = 0; i < 8; ++i) { g.setColour (juce::Colour::fromHSV (i / 8.0f, 0.8f, 1.0f, 1.0f)); g.fillRect (i * 80, 0, 80, 360); }
            g.setColour (juce::Colours::black);
            for (int y = 0; y < 360; y += 40) g.fillRect (0, y, 640, 6);
        }
        juce::PNGImageFormat png;
        auto pf = outDir.getChildFile ("pattern.png");
        pf.deleteFile();
        { juce::FileOutputStream os (pf); png.writeImageToStream (pat, os); }

        window = std::make_unique<juce::DocumentWindow> ("render test", juce::Colours::black, 0);
        window->setUsingNativeTitleBar (true);
        window->setContentOwned (new TestComponent (outDir, args.size() > 1 ? args[1].unquoted() : juce::String(), args.size() > 2 ? args[2].unquoted() : juce::String()), true);
        window->setVisible (true);
    }
    void shutdown() override { window.reset(); }
private:
    std::unique_ptr<juce::DocumentWindow> window;
};

START_JUCE_APPLICATION (RenderTestApp)
