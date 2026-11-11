#include "RenderEngine.h"
#include "EffectLibrary.h"
#include "ShaderPrelude.h"
#include "VizLibrary.h"
#include "model/Scene.h"

namespace moshdeck
{
using namespace juce::gl;

enum TexUnit { tuIn = 0, tuPrev, tuPrevIn, tuDeckA, tuDeckB, tuDeckAPrev, tuDeckBPrev, tuBelow, tuAudio, tuSpec, tuCount };

static const char* deckFitBody = R"GLSL(
uniform vec2 uSrcSize;
uniform float uHas;
void main() {
    float ca = uRes.x / uRes.y, ta = uSrcSize.x / max(uSrcSize.y, 1.0);
    vec2 uv = vUv;
    if (ta > ca) uv.x = (uv.x - 0.5) * ca / ta + 0.5; else uv.y = (uv.y - 0.5) * ta / ca + 0.5;
    fragColor = uHas > 0.5 ? vec4(texture(uIn, uv).rgb, 1.0) : vec4(0.0, 0.0, 0.0, 1.0);
})GLSL";

static const char* mixBody = R"GLSL(
void main() { fragColor = mix(texture(uDeckA, vUv), texture(uDeckB, vUv), uMix); })GLSL";

static const char* xformBody = R"GLSL(
void main() {
    vec2 p = aspectUv(vUv) - vec2(P1, P2) * vec2(uRes.x / uRes.y, 1.0) * 0.5;
    p = rot2(P3) * p / (max(P0, 0.01) * (1.0 + uBass * P4 * 0.5));
    vec2 q = p * vec2(uRes.y / uRes.x, 1.0) + 0.5;
    if (P5 > 0.5) q = abs(mod(q + 1.0, 2.0) - 1.0);
    else if (q.x < 0.0 || q.y < 0.0 || q.x > 1.0 || q.y > 1.0) { fragColor = vec4(0.0); return; }
    fragColor = vec4(texture(uIn, q).rgb, 1.0);
})GLSL";

static const char* blendBody = R"GLSL(
uniform int uMode;
vec3 blendRGB(vec3 b, vec3 t, int m) {
    if (m == 1) return b + t;
    if (m == 2) return 1.0 - (1.0 - b) * (1.0 - t);
    if (m == 3) return b * t;
    if (m == 4) return mix(2.0 * b * t, 1.0 - 2.0 * (1.0 - b) * (1.0 - t), step(0.5, b));
    if (m == 5) return mix(2.0 * b * t + b * b * (1.0 - 2.0 * t), sqrt(max(b, 0.0)) * (2.0 * t - 1.0) + 2.0 * b * (1.0 - t), step(0.5, t));
    if (m == 6) return mix(2.0 * b * t, 1.0 - 2.0 * (1.0 - b) * (1.0 - t), step(0.5, t));
    if (m == 7) return abs(b - t);
    if (m == 8) return b + t - 2.0 * b * t;
    if (m == 9) return max(b - t, 0.0);
    if (m == 10) return max(b, t);
    if (m == 11) return min(b, t);
    if (m == 12) return min(b / max(1.0 - t, 1e-3), 1.0);
    if (m == 13) return 1.0 - min((1.0 - b) / max(t, 1e-3), 1.0);
    if (m == 14) return clamp(b + 2.0 * t - 1.0, 0.0, 1.0);
    if (m == 15) { vec3 hb = rgb2hsv(b), ht = rgb2hsv(t); return hsv2rgb(vec3(ht.x, hb.y, hb.z)); }
    if (m == 16) { vec3 hb = rgb2hsv(b), ht = rgb2hsv(t); return hsv2rgb(vec3(ht.x, ht.y, hb.z)); }
    if (m == 17) { vec3 hb = rgb2hsv(b), ht = rgb2hsv(t); return hsv2rgb(vec3(hb.x, hb.y, ht.z)); }
    return t;
}
void main() {
    vec4 b = texture(uBelow, vUv), t = texture(uIn, vUv);
    float a = t.a;
    vec3 tc = t.rgb;
    if (uMode == 18) a = luma(t.rgb);
    else if (uMode == 19) a = 1.0 - luma(t.rgb);
    else if (uMode == 20) tc = t.rgb / max(t.a, 1e-3);
    else if (uMode > 0) tc = blendRGB(b.rgb, t.rgb, uMode);
    fragColor = vec4(mix(b.rgb, tc, clamp(a * uMix, 0.0, 1.0)), 1.0);
})GLSL";

static const char* masterBody = R"GLSL(
void main() { fragColor = vec4(texture(uIn, vUv).rgb * uMix, 1.0); })GLSL";

static const char* copyBody = R"GLSL(
void main() { fragColor = vec4(texture(uIn, vUv).rgb, 1.0); })GLSL";

//==============================================================================
RenderEngine::RenderEngine (EngineHost& h) : host (h) {}
RenderEngine::~RenderEngine() = default;

void RenderEngine::contextCreated()
{
    geo.create();
    juce::String log;
    if (! milkShaders.create (log))
        shaderErrors["milk"] = log;

    const float black[4] = { 0, 0, 0, 0 };
    blackTex.uploadRGBA (black, 1, 1);
    std::vector<uint8_t> noise (256 * 256 * 4);
    juce::Random r (1234);
    for (auto& n : noise) n = (uint8_t) r.nextInt (256);
    noiseTex.uploadRGBA (noise.data(), 256, 256);
    glBindTexture (GL_TEXTURE_2D, noiseTex.tex);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    startTime = lastTime = juce::Time::getMillisecondCounterHiRes() * 0.001;
    ready = true;
}

void RenderEngine::contextClosing()
{
    ready = false;
    layers.clear();
    masterFx.clear();
    shaders.clear();
    milkShaders.release();
    for (auto* t : { &comp[0], &comp[1], &masterChain[0], &masterChain[1], &output, &deckMix, &thumbTarget,
                     &deckFit[0][0], &deckFit[0][1], &deckFit[1][0], &deckFit[1][1] })
        t->release();
    for (auto* t : { &deckTex[0], &deckTex[1], &audioTex, &specTex, &blackTex, &noiseTex })
        t->release();
    deckSerial[0] = deckSerial[1] = 0;
    geo.release();
}

//==============================================================================
static void initSamplers (gl::Shader& s)
{
    s.use();
    s.set1i ("uIn", tuIn); s.set1i ("uPrev", tuPrev); s.set1i ("uPrevIn", tuPrevIn);
    s.set1i ("uDeckA", tuDeckA); s.set1i ("uDeckB", tuDeckB); s.set1i ("uDeckAPrev", tuDeckAPrev); s.set1i ("uDeckBPrev", tuDeckBPrev);
    s.set1i ("uBelow", tuBelow); s.set1i ("uAudio", tuAudio); s.set1i ("uSpec", tuSpec);
}

gl::Shader* RenderEngine::builtin (const juce::String& key, const juce::String& body)
{
    auto it = shaders.find (key);
    if (it != shaders.end()) return it->second->valid() ? it->second.get() : nullptr;
    auto s = std::make_unique<gl::Shader>();
    juce::String log;
    if (! s->build (shaders::fullscreenVS, juce::String (shaders::prelude) + body, log))
    {
        shaderErrors[key] = log;
        DBG ("Shader " << key << " failed: " << log);
    }
    else
        initSamplers (*s);
    auto* raw = s.get();
    shaders[key] = std::move (s);
    return raw->valid() ? raw : nullptr;
}

gl::Shader* RenderEngine::effectShader (const juce::String& id)
{
    auto* def = EffectLibrary::find (id);
    if (def == nullptr) return nullptr;
    const bool keying = def->category == "Keying";
    juce::String main = keying
        ? "\nvoid main() { vec4 o = texture(uIn, vUv); vec4 r = fx(vUv); fragColor = mix(o, r, uMix); }\n"
        : "\nvoid main() { vec4 o = texture(uIn, vUv); vec4 r = fx(vUv);"
          " r.a = max(o.a, clamp(max(r.r, max(r.g, r.b)), 0.0, 1.0)); fragColor = mix(o, r, uMix); }\n";
    return builtin ("fx:" + id, def->glsl + main);
}

gl::Shader* RenderEngine::generatorShader (int index)
{
    auto* def = GeneratorLibrary::get (index);
    if (def == nullptr) return nullptr;
    return builtin ("gen:" + def->id, def->glsl + "\nvoid main() { fragColor = gen(vUv); }\n");
}

gl::Shader* RenderEngine::vizShader (int type)
{
    return builtin ("viz:" + juce::String (type), VizLibrary::glsl (type) + "\nvoid main() { fragColor = gen(vUv); }\n");
}

float RenderEngine::val (const juce::String& id, float fallback) const
{
    const int i = scene.find (id);
    return i >= 0 && (size_t) i < finalValues.size() ? finalValues[(size_t) i] : fallback;
}

void RenderEngine::setCommon (gl::Shader& s, const float* p8, float mix, int localFrame)
{
    s.use();
    s.set2f ("uRes", (float) fs.W, (float) fs.H);
    s.set1f ("uTime", fs.time);
    s.set1f ("uBeat", fs.beat);
    s.set1f ("uBar", fs.bar);
    s.set1f ("uBeats", fs.beats);
    s.set1f ("uBpm", fs.bpm);
    s.set1fv ("uBands", 5, fs.bands);
    s.set1f ("uLevel", fs.level);
    s.set1f ("uKick", fs.kick);
    s.set1f ("uSnare", fs.snare);
    s.set1f ("uHat", fs.hat);
    s.set1f ("uPulse", fs.pulse);
    s.set1f ("uRand", fs.rnd);
    s.set1f ("uMix", mix);
    s.set1f ("uTrigBeat", fs.trigBeat);
    s.set1f ("uTrigBar", fs.trigBar);
    s.set1f ("uTrigKick", fs.trigKick);
    s.set1i ("uFrame", fs.frame);
    s.set1i ("uLocalFrame", localFrame);
    float zeros[8] = {};
    s.set1fv ("uP", 8, p8 != nullptr ? p8 : zeros);
}

void RenderEngine::bindCommonTextures (GLuint in, GLuint prev, GLuint prevIn, GLuint below)
{
    const int prevFit = 1 - deckFitCur;
    gl::bindTex (tuIn, in ? in : blackTex.tex);
    gl::bindTex (tuPrev, prev ? prev : blackTex.tex);
    gl::bindTex (tuPrevIn, prevIn ? prevIn : blackTex.tex);
    gl::bindTex (tuDeckA, deckFit[0][deckFitCur].tex);
    gl::bindTex (tuDeckB, deckFit[1][deckFitCur].tex);
    gl::bindTex (tuDeckAPrev, deckFit[0][prevFit].tex);
    gl::bindTex (tuDeckBPrev, deckFit[1][prevFit].tex);
    gl::bindTex (tuBelow, below ? below : blackTex.tex);
    gl::bindTex (tuAudio, audioTex.tex);
    gl::bindTex (tuSpec, specTex.tex);
    glActiveTexture (GL_TEXTURE0);
}

GLuint RenderEngine::inputFor (InputRoute r, GLuint below) const
{
    switch (r)
    {
        case InputRoute::DeckA:   return deckFit[0][deckFitCur].tex;
        case InputRoute::DeckB:   return deckFit[1][deckFitCur].tex;
        case InputRoute::DeckMix: return deckMix.tex;
        case InputRoute::Below:   return below;
        default: break;
    }
    return blackTex.tex;
}

void RenderEngine::blit (const gl::Target& src, gl::Target& dst)
{
    glBindFramebuffer (GL_READ_FRAMEBUFFER, src.fbo);
    glBindFramebuffer (GL_DRAW_FRAMEBUFFER, dst.fbo);
    glBlitFramebuffer (0, 0, src.w, src.h, 0, 0, dst.w, dst.h, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBindFramebuffer (GL_FRAMEBUFFER, 0);
}

//==============================================================================
void RenderEngine::updateDecks (double dt, const SceneData& sd, const std::vector<float>& vals)
{
    juce::ignoreUnused (vals);
    deckFitCur = 1 - deckFitCur;
    for (int d = 0; d < 2; ++d)
    {
        const juce::String p = d == 0 ? "DA" : "DB";
        auto& deck = host.getDeck (d);
        deck.tick (dt, val (p + ".speed", 1.0f), fs.beats, std::round (val (p + ".beats", 0.0f)), val (p + ".cue", 0.0f));

        const float nextBars = std::round (val (p + ".next", 0.0f));
        if (nextBars > 0 && sd.decks[d].files.size() > 1)
        {
            const double slot = std::floor (fs.beats / (4.0 * nextBars));
            if (lastDeckBar[d] >= 0 && slot != lastDeckBar[d])
                host.onDeckAdvance (d);
            lastDeckBar[d] = slot;
        }

        if (deck.fetch (deckFrames[d], deckSerial[d]))
        {
            deckSerial[d] = deckFrames[d].serial;
            deckTex[d].uploadRGBA (deckFrames[d].rgba.data(), deckFrames[d].width, deckFrames[d].height);
            deckHasFrame[d] = true;
        }
        if (deck.getPath().isEmpty()) deckHasFrame[d] = false;

        auto& t = deckFit[d][deckFitCur];
        t.ensure (fs.W, fs.H);
        deckFit[d][1 - deckFitCur].ensure (fs.W, fs.H);
        if (auto* s = builtin ("deckfit", deckFitBody))
        {
            t.bind();
            glDisable (GL_BLEND);
            setCommon (*s, nullptr, 1.0f, 0);
            s->set2f ("uSrcSize", (float) juce::jmax (1, deckTex[d].w), (float) juce::jmax (1, deckTex[d].h));
            s->set1f ("uHas", deckHasFrame[d] ? 1.0f : 0.0f);
            bindCommonTextures (deckTex[d].tex ? deckTex[d].tex : blackTex.tex, 0, 0, 0);
            geo.drawQuad();
        }
    }
    deckMix.ensure (fs.W, fs.H);
    if (auto* s = builtin ("deckmix", mixBody))
    {
        deckMix.bind();
        setCommon (*s, nullptr, val (Scene::crossfaderId, 0.0f), 0);
        bindCommonTextures (0, 0, 0, 0);
        geo.drawQuad();
    }
}

//==============================================================================
void RenderEngine::renderShadertoy (LayerRuntime& rt, const Layer& l, const std::vector<float>& vals, GLuint input, GLuint below, gl::Target& dest)
{
    juce::ignoreUnused (vals);
    const auto code = l.shaderCode.isEmpty() ? ShadertoyProject::defaultShader() : l.shaderCode;
    const auto hash = juce::String (code.hashCode64());
    if (rt.st == nullptr || rt.st->sourceHash != hash)
    {
        rt.st = std::make_unique<ShadertoyRuntime>();
        rt.st->sourceHash = hash;
        juce::String err;
        rt.st->project = ShadertoyProject::parse (code, err);
        juce::String log = err;
        bool ok = err.isEmpty() && rt.st->project.isValid();
        if (ok)
            for (auto& pass : rt.st->project.passes)
            {
                auto sh = std::make_unique<gl::Shader>();
                juce::String plog;
                if (! sh->build (shaders::fullscreenVS, rt.st->project.buildFragmentSource (pass), plog))
                {
                    ok = false;
                    log << pass.name << ": " << plog << "\n";
                }
                rt.st->shaders.push_back (std::move (sh));
            }
        rt.st->ok = ok;
        host.setLayerStatus (l.uid, ok ? "Shadertoy OK: " + juce::String ((int) rt.st->project.passes.size()) + " pass(es)" : "Shadertoy error:\n" + log);
    }
    auto& st = *rt.st;
    if (! st.ok) { dest.clear (0, 0, 0, 1); return; }

    const float iParam[4] = { val (l.uid + ".p0"), val (l.uid + ".p1"), val (l.uid + ".p2"), val (l.uid + ".p3") };
    const float mx = val (l.uid + ".p4") * (float) fs.W, my = val (l.uid + ".p5") * (float) fs.H;
    const bool mdown = val (l.uid + ".p6") > 0.5f;
    const auto now = juce::Time::getCurrentTime();

    for (size_t i = 0; i < st.project.passes.size(); ++i)
    {
        const auto& pass = st.project.passes[i];
        auto& sh = *st.shaders[i];
        gl::Target* target = &dest;
        if (pass.output >= 0)
        {
            auto& pair = st.buffers[pass.output];
            pair[0].ensure (fs.W, fs.H); pair[1].ensure (fs.W, fs.H);
            target = &pair[1 - st.latest[pass.output]];
        }
        target->bind();
        glDisable (GL_BLEND);
        sh.use();
        sh.set3f ("iResolution", (float) fs.W, (float) fs.H, 1.0f);
        sh.set1f ("iTime", (float) rt.time);
        sh.set1f ("iTimeDelta", 1.0f / 60.0f);
        sh.set1f ("iFrameRate", (float) juce::jmax (1.0, fps.load()));
        sh.set1f ("iSampleRate", (float) frameIn.mod.audio.sampleRate);
        sh.set1i ("iFrame", st.frame);
        sh.set4f ("iMouse", mx, my, mdown ? mx : -mx, mdown ? my : -my);
        sh.set4f ("iDate", (float) now.getYear(), (float) now.getMonth(), (float) now.getDayOfMonth(),
                  (float) (now.getHours() * 3600 + now.getMinutes() * 60 + now.getSeconds()) + (float) now.getMilliseconds() * 0.001f);
        sh.set1f ("iParam0", iParam[0]); sh.set1f ("iParam1", iParam[1]); sh.set1f ("iParam2", iParam[2]); sh.set1f ("iParam3", iParam[3]);
        sh.set1f ("iBass", fs.bands[1]); sh.set1f ("iMid", fs.bands[2]); sh.set1f ("iTreble", fs.bands[4]);
        sh.set1f ("iLevel", fs.level); sh.set1f ("iKick", fs.kick); sh.set1f ("iBeat", fs.beat); sh.set1f ("iBpm", fs.bpm);
        float res[12];
        for (int c = 0; c < 4; ++c)
        {
            const auto& ch = pass.channels[(size_t) c];
            GLuint tex = blackTex.tex;
            int tw = fs.W, th = fs.H;
            using K = ShadertoyProject::ChannelKind;
            switch (ch.kind)
            {
                case K::Input: tex = input; break;
                case K::DeckA: tex = deckFit[0][deckFitCur].tex; break;
                case K::DeckB: tex = deckFit[1][deckFitCur].tex; break;
                case K::Below: tex = below; break;
                case K::Audio: tex = audioTex.tex; tw = 512; th = 2; break;
                case K::Noise: tex = noiseTex.tex; tw = th = 256; break;
                case K::Self:
                    if (pass.output >= 0) tex = st.buffers[pass.output][st.latest[pass.output]].tex;
                    break;
                case K::Buffer:
                    if (ch.buffer >= 0 && st.buffers[ch.buffer][0].tex != 0) tex = st.buffers[ch.buffer][st.latest[ch.buffer]].tex;
                    break;
                default: break;
            }
            gl::bindTex (c, tex ? tex : blackTex.tex);
            if (ch.kind != ShadertoyProject::ChannelKind::Noise && ch.kind != ShadertoyProject::ChannelKind::Audio && tex != blackTex.tex)
            {
                glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, ch.repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
                glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, ch.repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
            }
            const juce::String nm = "iChannel" + juce::String (c);
            sh.set1i (nm.toRawUTF8(), c);
            res[c * 3] = (float) tw; res[c * 3 + 1] = (float) th; res[c * 3 + 2] = 1.0f;
        }
        const GLint rl = sh.loc ("iChannelResolution");
        if (rl >= 0) glUniform3fv (rl, 4, res);
        geo.drawQuad();
        if (pass.output >= 0) st.latest[pass.output] = 1 - st.latest[pass.output];
        // restore clamp on shared targets
        for (int c = 0; c < 4; ++c)
        {
            glActiveTexture ((GLenum) (GL_TEXTURE0 + c));
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
        glActiveTexture (GL_TEXTURE0);
    }
    ++st.frame;
}

void RenderEngine::renderSource (LayerRuntime& rt, const Layer& l, const std::vector<float>& vals, GLuint input, GLuint below, double dt)
{
    rt.time += dt * val (l.uid + ".speed", 1.0f);
    float p[8] = {};
    for (int k = 0; k < 8; ++k) p[k] = val (l.uid + ".p" + juce::String (k));
    rt.src.ensure (fs.W, fs.H);
    const float savedTime = fs.time;
    fs.time = (float) rt.time;
    glDisable (GL_BLEND);

    switch (l.source)
    {
        case SourceType::DeckA: case SourceType::DeckB: case SourceType::DeckMix: case SourceType::Below:
        {
            GLuint tex = l.source == SourceType::DeckA ? deckFit[0][deckFitCur].tex
                       : l.source == SourceType::DeckB ? deckFit[1][deckFitCur].tex
                       : l.source == SourceType::DeckMix ? deckMix.tex : below;
            if (auto* s = builtin ("xform", xformBody))
            {
                rt.src.bind();
                setCommon (*s, p, 1.0f, rt.localFrame);
                bindCommonTextures (tex, 0, 0, below);
                geo.drawQuad();
            }
            break;
        }
        case SourceType::Generator:
        {
            auto* def = GeneratorLibrary::get (l.sourceIndex);
            if (auto* s = generatorShader (l.sourceIndex))
            {
                if (def != nullptr && def->usesHistory)
                {
                    rt.genPrev[0].ensure (fs.W, fs.H); rt.genPrev[1].ensure (fs.W, fs.H);
                    auto& dst = rt.genPrev[1 - rt.genCur];
                    dst.bind();
                    setCommon (*s, p, 1.0f, rt.localFrame);
                    bindCommonTextures (input, rt.genPrev[rt.genCur].tex, 0, below);
                    geo.drawQuad();
                    rt.genCur = 1 - rt.genCur;
                    blit (dst, rt.src);
                }
                else
                {
                    rt.src.bind();
                    setCommon (*s, p, 1.0f, rt.localFrame);
                    bindCommonTextures (input, 0, 0, below);
                    geo.drawQuad();
                }
            }
            else
                rt.src.clear (0, 0, 0, 1);
            break;
        }
        case SourceType::AudioViz:
        {
            if (auto* s = vizShader (l.sourceIndex))
            {
                rt.src.bind();
                setCommon (*s, p, 1.0f, rt.localFrame);
                bindCommonTextures (input, 0, 0, below);
                geo.drawQuad();
            }
            break;
        }
        case SourceType::Milk:
        {
            if (rt.milk == nullptr) rt.milk = std::make_unique<MilkSource>();
            rt.milk->setPreset (l.milkPreset.isEmpty() ? juce::String ("builtin:Liquid Mirror") : l.milkPreset);
            MilkFrameParams mp;
            const auto& a = frameIn.mod.audio;
            mp.in.time = rt.time;
            mp.in.dt = dt;
            mp.in.fps = juce::jmax (1.0, fps.load());
            mp.in.frame = rt.localFrame;
            mp.in.bass = 0.3f + a.bandsFast[1] * 1.7f;  mp.in.bassAtt = 0.3f + a.bands[1] * 1.7f;
            mp.in.mid  = 0.3f + a.bandsFast[2] * 1.7f;  mp.in.midAtt  = 0.3f + a.bands[2] * 1.7f;
            mp.in.treb = 0.3f + a.bandsFast[4] * 1.7f;  mp.in.trebAtt = 0.3f + a.bands[4] * 1.7f;
            mp.in.aspectX = fs.W > fs.H ? (float) fs.H / fs.W : 1.0f;
            mp.in.aspectY = fs.H > fs.W ? (float) fs.W / fs.H : 1.0f;
            mp.in.pixelsX = fs.W; mp.in.pixelsY = fs.H;
            mp.in.zoomOffset = p[2]; mp.in.rotOffset = p[3]; mp.in.warpScale = p[4];
            mp.in.decayOffset = p[5]; mp.in.waveScale = p[6];
            static thread_local std::array<float, 512> wave, spec;
            for (int i = 0; i < 512; ++i) { wave[(size_t) i] = a.wave[(size_t) i]; spec[(size_t) i] = a.spectrum[(size_t) i]; }
            mp.wave512 = wave.data(); mp.spectrum512 = spec.data();
            mp.inputTex = input;
            mp.inject = p[0];
            mp.injectMode = (int) std::round (p[1]);
            mp.beatPulse = fs.pulse; mp.kick = fs.kick;
            mp.width = fs.W; mp.height = fs.H;
            rt.milk->render (milkShaders, geo, mp, rt.src);

            const float autoBeats = std::round (p[7]);
            if (autoBeats > 0.0f)
            {
                const double slot = std::floor (fs.beats / autoBeats);
                if (rt.lastAutoBeat >= 0 && slot != rt.lastAutoBeat)
                    host.onMilkAdvance (l.uid);
                rt.lastAutoBeat = slot;
            }
            juce::String status = "Milk: " + rt.milk->currentSpec().fromFirstOccurrenceOf ("builtin:", false, false);
            if (rt.milk->hasPixelShaders) status << "\n(note: preset's HLSL warp/comp shaders are not executed; classic pipeline only)";
            if (rt.milk->lastErrors.isNotEmpty()) status << "\nEquation errors:\n" << rt.milk->lastErrors;
            host.setLayerStatus (l.uid, status);
            break;
        }
        case SourceType::Shadertoy:
            renderShadertoy (rt, l, vals, input, below, rt.src);
            break;
        default:
            rt.src.clear (0, 0, 0, 0);
            break;
    }
    fs.time = savedTime;
    ++rt.localFrame;
}


GLuint RenderEngine::runChain (const juce::String& owner, std::map<juce::String, FxRuntime>& fxs, const std::vector<EffectSlot>& chain,
                               gl::Target* pingPong, GLuint input, gl::Target* inputTarget, const std::vector<float>& vals, GLuint below)
{
    juce::ignoreUnused (vals, input);
    gl::Target* current = inputTarget;
    int pp = 0;
    // drop runtimes of removed effects
    for (auto it = fxs.begin(); it != fxs.end();)
    {
        const bool exists = std::any_of (chain.begin(), chain.end(), [&] (const EffectSlot& e) { return e.uid == it->first; });
        it = exists ? std::next (it) : fxs.erase (it);
    }
    for (auto& slot : chain)
    {
        if (! slot.enabled) continue;
        auto* def = EffectLibrary::find (slot.effectId);
        auto* sh = effectShader (slot.effectId);
        if (def == nullptr || sh == nullptr) continue;
        const juce::String base = owner + "." + slot.uid + ".";
        const float mix = val (base + "mix", 1.0f);
        if (mix <= 0.0001f) continue;
        float p[8] = {};
        for (size_t k = 0; k < def->params.size() && k < 8; ++k) p[k] = val (base + def->params[k].key);
        glDisable (GL_BLEND);
        if (def->usesHistory)
        {
            auto& fr = fxs[slot.uid];
            fr.out[0].ensure (fs.W, fs.H); fr.out[1].ensure (fs.W, fs.H); fr.prevIn.ensure (fs.W, fs.H);
            auto& dst = fr.out[1 - fr.cur];
            dst.bind();
            setCommon (*sh, p, mix, fr.localFrame);
            bindCommonTextures (current->tex, fr.out[fr.cur].tex, fr.prevIn.tex, below);
            geo.drawQuad();
            blit (*current, fr.prevIn);
            fr.cur = 1 - fr.cur;
            ++fr.localFrame;
            current = &dst;
        }
        else
        {
            if (current == &pingPong[pp]) pp = 1 - pp;
            auto& dst = pingPong[pp];
            dst.ensure (fs.W, fs.H);
            dst.bind();
            setCommon (*sh, p, mix, 0);
            bindCommonTextures (current->tex, 0, 0, below);
            geo.drawQuad();
            current = &dst;
            pp = 1 - pp;
        }
    }
    return current->tex;
}

//==============================================================================
void RenderEngine::renderFrame (int vw, int vh, float scale)
{
    juce::ignoreUnused (scale);
    if (! ready) return;
    GLint defaultFbo = 0;
    glGetIntegerv (GL_FRAMEBUFFER_BINDING, &defaultFbo);

    // ---- timing
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const double dt = juce::jlimit (0.0, 0.25, now - lastTime);
    lastTime = now;
    if (dt > 0) fps = fps.load() * 0.95 + (1.0 / dt) * 0.05;

    // ---- gather inputs
    host.buildFrameInput (frameIn);
    host.getCanvasSize (fs.W, fs.H);
    sceneVersion = host.getScene().snapshot (scene, sceneVersion);
    modEval.evaluate (scene, sceneVersion, frameIn.mod, finalValues);
    host.getScene().publishModulated (finalValues, sceneVersion);

    const auto& a = frameIn.mod.audio;
    const double beats = frameIn.mod.beats;
    fs.time = (float) (now - startTime);
    fs.beats = (float) beats;
    fs.beat = (float) (beats - std::floor (beats));
    const double bars = beats / juce::jmax (1, frameIn.mod.beatsPerBar);
    fs.bar = (float) (bars - std::floor (bars));
    fs.bpm = (float) frameIn.mod.bpm;
    for (int i = 0; i < 5; ++i) fs.bands[i] = a.bands[(size_t) i];
    fs.level = a.level; fs.kick = a.kick; fs.snare = a.snare; fs.hat = a.hat;
    fs.trigBeat = std::floor (beats) != std::floor (prevBeats) ? 1.0f : 0.0f;
    fs.trigBar = std::floor (bars) != std::floor (prevBeats / juce::jmax (1, frameIn.mod.beatsPerBar)) ? 1.0f : 0.0f;
    fs.trigKick = a.kickCount != prevKickCount ? 1.0f : 0.0f;
    prevKickCount = a.kickCount;
    prevBeats = beats;
    pulse = fs.trigBeat > 0 ? 1.0f : pulse * (float) std::pow (0.02, dt);
    fs.pulse = frameIn.mod.beatPulse > 0 ? frameIn.mod.beatPulse : pulse;
    fs.rnd = juce::Random::getSystemRandom().nextFloat();
    fs.frame = frameCounter++;

    // ---- audio textures
    {
        std::array<float, 1024> at {};
        for (int i = 0; i < 512; ++i) { at[(size_t) i] = a.spectrum[(size_t) i]; at[(size_t) (512 + i)] = a.wave[(size_t) i] * 0.5f + 0.5f; }
        audioTex.uploadFloat (at.data(), 512, 2);
        std::array<float, 256> st {};
        for (int i = 0; i < 128; ++i) { st[(size_t) i] = a.logSpectrum[(size_t) i]; st[(size_t) (128 + i)] = a.logPeaks[(size_t) i]; }
        specTex.uploadFloat (st.data(), 128, 2);
    }

    updateDecks (dt, scene, finalValues);

    // ---- layers
    for (auto it = layers.begin(); it != layers.end();)
    {
        const bool exists = std::any_of (scene.layers.begin(), scene.layers.end(), [&] (const Layer& l) { return l.uid == it->first; });
        it = exists ? std::next (it) : layers.erase (it);
    }
    comp[0].ensure (fs.W, fs.H); comp[1].ensure (fs.W, fs.H);
    int cc = 0;
    comp[cc].clear (0, 0, 0, 1);
    const bool anySolo = std::any_of (scene.layers.begin(), scene.layers.end(), [] (const Layer& l) { return l.solo && l.enabled; });

    for (auto& l : scene.layers)
    {
        if (! l.enabled || (anySolo && ! l.solo)) continue;
        const float opacity = val (l.uid + ".opacity", 1.0f);
        if (opacity <= 0.001f) continue;
        auto& rtPtr = layers[l.uid];
        if (rtPtr == nullptr) rtPtr = std::make_unique<LayerRuntime>();
        auto& rt = *rtPtr;
        const GLuint below = comp[cc].tex;
        const GLuint in = inputFor (l.input, below);
        renderSource (rt, l, finalValues, in, below, dt);
        rt.chain[0].ensure (fs.W, fs.H); rt.chain[1].ensure (fs.W, fs.H);
        const GLuint result = runChain (l.uid, rt.fx, l.effects, rt.chain, rt.src.tex, &rt.src, finalValues, below);

        if (auto* s = builtin ("blend", blendBody))
        {
            comp[1 - cc].bind();
            glDisable (GL_BLEND);
            setCommon (*s, nullptr, opacity, 0);
            s->set1i ("uMode", l.blendMode);
            bindCommonTextures (result, 0, 0, below);
            geo.drawQuad();
            cc = 1 - cc;
        }
    }

    // ---- master chain + brightness
    masterChain[0].ensure (fs.W, fs.H); masterChain[1].ensure (fs.W, fs.H);
    const GLuint masterTex = runChain ("master", masterFx, scene.masterFx, masterChain, comp[cc].tex, &comp[cc], finalValues, comp[cc].tex);
    output.ensure (fs.W, fs.H);
    if (auto* s = builtin ("master", masterBody))
    {
        output.bind();
        setCommon (*s, nullptr, val (Scene::masterId, 1.0f), 0);
        bindCommonTextures (masterTex, 0, 0, 0);
        geo.drawQuad();
    }

    // ---- thumbnail for a secondary preview
    if (wantThumb && (fs.frame % 3) == 0)
    {
        thumbTarget.ensure (320, 180);
        blit (output, thumbTarget);
        glBindFramebuffer (GL_FRAMEBUFFER, thumbTarget.fbo);
        thumbPixels.resize (320 * 180 * 4);
        glPixelStorei (GL_PACK_ALIGNMENT, 1);
        glReadPixels (0, 0, 320, 180, GL_RGBA, GL_UNSIGNED_BYTE, thumbPixels.data());
        juce::Image img (juce::Image::RGB, 320, 180, false);
        {
            juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
            for (int y = 0; y < 180; ++y)
                for (int x = 0; x < 320; ++x)
                {
                    const auto* px = &thumbPixels[(size_t) (((179 - y) * 320 + x) * 4)];
                    bd.setPixelColour (x, y, juce::Colour (px[0], px[1], px[2]));
                }
        }
        const juce::SpinLock::ScopedLockType sl (thumbLock);
        thumb = img;
    }

    // ---- present (letterboxed)
    glBindFramebuffer (GL_FRAMEBUFFER, (GLuint) defaultFbo);
    glViewport (0, 0, vw, vh);
    glClearColor (0, 0, 0, 1);
    glClear (GL_COLOR_BUFFER_BIT);
    const float ca = (float) fs.W / fs.H, va = (float) vw / juce::jmax (1, vh);
    int w = vw, h = vh;
    if (va > ca) w = (int) (vh * ca); else h = (int) (vw / ca);
    glViewport ((vw - w) / 2, (vh - h) / 2, w, h);
    if (auto* s = builtin ("copy", copyBody))
    {
        setCommon (*s, nullptr, 1.0f, 0);
        bindCommonTextures (output.tex, 0, 0, 0);
        geo.drawQuad();
    }
    glBindVertexArray (0);
}

bool RenderEngine::getThumbnail (juce::Image& dest)
{
    const juce::SpinLock::ScopedLockType sl (thumbLock);
    if (! thumb.isValid()) return false;
    dest = thumb;
    return true;
}
} // namespace moshdeck
