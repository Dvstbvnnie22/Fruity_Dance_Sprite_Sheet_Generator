#include "MilkRenderer.h"

namespace moshdeck
{
using namespace juce::gl;

static const char* milkVS = R"GLSL(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUv;
layout(location = 2) in vec4 aCol;
out vec2 vUv; out vec4 vCol; out vec2 vScreen;
void main() { vUv = aUv; vCol = aCol; vScreen = aPos * 0.5 + 0.5; gl_Position = vec4(aPos, 0.0, 1.0); }
)GLSL";

static const char* warpFS = R"GLSL(#version 330 core
in vec2 vUv; in vec4 vCol; in vec2 vScreen;
out vec4 fragColor;
uniform sampler2D uState, uInput;
uniform float uDecay, uInject, uPulse, uKick;
uniform int uInjectMode, uWrap;
void main() {
    vec2 uv = uWrap == 1 ? fract(vUv) : clamp(vUv, 0.0, 1.0);
    vec3 c = texture(uState, uv).rgb * uDecay;
    c = max(c - 1.0 / 512.0, 0.0); // MilkDrop-style dark bias so colours fade fully
    vec3 base = texture(uInput, vScreen).rgb;
    float k = uInject;
    if (uInjectMode == 1) k *= uPulse;
    else if (uInjectMode == 2) k *= uKick;
    else if (uInjectMode == 3) k *= dot(base, vec3(0.299, 0.587, 0.114));
    fragColor = vec4(mix(c, base, clamp(k, 0.0, 1.0)), 1.0);
}
)GLSL";

static const char* drawFS = R"GLSL(#version 330 core
in vec2 vUv; in vec4 vCol; in vec2 vScreen;
out vec4 fragColor;
uniform sampler2D uTex;
uniform int uTextured;
void main() {
    vec4 c = vCol;
    if (uTextured == 1) c.rgb *= texture(uTex, vUv).rgb;
    fragColor = c;
}
)GLSL";

static const char* compFS = R"GLSL(#version 330 core
in vec2 vUv; in vec4 vCol; in vec2 vScreen;
out vec4 fragColor;
uniform sampler2D uState;
uniform float uGamma, uEchoZoom, uEchoAlpha;
uniform int uEchoOrient, uDarkenCenter, uBrighten, uDarken, uSolarize, uInvert;
void main() {
    vec2 uv = vScreen;
    vec3 c = texture(uState, uv).rgb;
    if (uEchoAlpha > 0.001) {
        vec2 e = (uv - 0.5) / max(uEchoZoom, 0.01) + 0.5;
        if (uEchoOrient == 1 || uEchoOrient == 3) e.x = 1.0 - e.x;
        if (uEchoOrient == 2 || uEchoOrient == 3) e.y = 1.0 - e.y;
        c = mix(c, texture(uState, e).rgb, uEchoAlpha);
    }
    c *= uGamma;
    if (uDarkenCenter == 1) c *= mix(0.6, 1.0, smoothstep(0.0, 0.35, length(uv - 0.5)));
    if (uBrighten == 1) c = sqrt(max(c, 0.0));
    if (uDarken == 1) c = c * c;
    if (uSolarize == 1) c = c * (1.0 - c) * 4.0;
    if (uInvert == 1) c = 1.0 - c;
    fragColor = vec4(clamp(c, 0.0, 1.0), 1.0);
}
)GLSL";

static const char* copyFS = R"GLSL(#version 330 core
in vec2 vUv; in vec4 vCol; in vec2 vScreen;
out vec4 fragColor;
uniform sampler2D uA, uB;
uniform float uMix;
void main() { fragColor = mix(texture(uA, vScreen), texture(uB, vScreen), uMix); }
)GLSL";

bool MilkShaders::create (juce::String& log)
{
    bool ok = warp.build (milkVS, warpFS, log);
    ok = draw.build (milkVS, drawFS, log) && ok;
    ok = comp.build (milkVS, compFS, log) && ok;
    ok = copy.build (milkVS, copyFS, log) && ok;
    return ok;
}

void MilkShaders::release() { warp.release(); draw.release(); comp.release(); copy.release(); }

//==============================================================================
static inline void pushV (std::vector<float>& v, float x, float y, float u, float t, float r, float g, float b, float a)
{
    v.insert (v.end(), { x, y, u, t, r, g, b, a });
}

/** Appends a screen-space thick line segment (in 0..1 coords, y up) as two triangles. */
static void appendLine (std::vector<float>& v, float x0, float y0, float x1, float y1, float widthPx, int W, int H, const float* c0, const float* c1)
{
    const float dx = (x1 - x0) * W, dy = (y1 - y0) * H;
    const float len = std::sqrt (dx * dx + dy * dy);
    if (len < 1.0e-4f) return;
    const float nx = -dy / len * widthPx * 0.5f / W, ny = dx / len * widthPx * 0.5f / H;
    auto P = [&] (float x, float y, const float* c) { pushV (v, x * 2 - 1, y * 2 - 1, 0, 0, c[0], c[1], c[2], c[3]); };
    P (x0 - nx, y0 - ny, c0); P (x0 + nx, y0 + ny, c0); P (x1 + nx, y1 + ny, c1);
    P (x0 - nx, y0 - ny, c0); P (x1 + nx, y1 + ny, c1); P (x1 - nx, y1 - ny, c1);
}

static void appendDot (std::vector<float>& v, float x, float y, float sizePx, int W, int H, const float* c)
{
    const float sx = sizePx * 0.5f / W, sy = sizePx * 0.5f / H;
    auto P = [&] (float px, float py) { pushV (v, px * 2 - 1, py * 2 - 1, 0, 0, c[0], c[1], c[2], c[3]); };
    P (x - sx, y - sy); P (x + sx, y - sy); P (x + sx, y + sy);
    P (x - sx, y - sy); P (x + sx, y + sy); P (x - sx, y + sy);
}

static void setBlend (bool additive)
{
    glEnable (GL_BLEND);
    if (additive) glBlendFunc (GL_SRC_ALPHA, GL_ONE);
    else          glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

//==============================================================================
bool MilkInstance::load (const milk::MilkPresetData& d, juce::String& errors)
{
    return state.load (d, &errors);
}

void MilkInstance::seedFrom (MilkShaders& sh, gl::Geometry& geo, const MilkInstance& other)
{
    if (other.buf[other.cur].tex == 0) return;
    const int W = other.buf[other.cur].w, H = other.buf[other.cur].h;
    buf[0].ensure (W, H); buf[1].ensure (W, H);
    buf[cur].bind();
    glDisable (GL_BLEND);
    sh.copy.use();
    sh.copy.set1i ("uA", 0); sh.copy.set1i ("uB", 0); sh.copy.set1f ("uMix", 0.0f);
    gl::bindTex (0, other.buf[other.cur].tex);
    verts.clear();
    pushV (verts, -1, -1, 0, 0, 1, 1, 1, 1); pushV (verts, 1, -1, 1, 0, 1, 1, 1, 1); pushV (verts, -1, 1, 0, 1, 1, 1, 1, 1);
    pushV (verts, 1, -1, 1, 0, 1, 1, 1, 1); pushV (verts, 1, 1, 1, 1, 1, 1, 1, 1); pushV (verts, -1, 1, 0, 1, 1, 1, 1, 1);
    geo.drawDynamic (verts, GL_TRIANGLES);
}

void MilkInstance::render (MilkShaders& sh, gl::Geometry& geo, const MilkFrameParams& p)
{
    const int W = p.width, H = p.height;
    const bool wrap = state.get ("wrap") != 0.0;
    for (auto& b : buf) b.ensure (W, H, wrap);
    out.ensure (W, H);

    state.runFrame (p.in, p.wave512, p.spectrum512);

    const int src = cur, dst = 1 - cur;

    // ---- 1. warp the previous frame through the per-vertex mesh, inject base input
    buf[dst].bind();
    glDisable (GL_BLEND);
    sh.warp.use();
    sh.warp.set1i ("uState", 0);
    sh.warp.set1i ("uInput", 1);
    sh.warp.set1f ("uDecay", (float) juce::jlimit (0.0, 1.0, state.get ("decay")));
    sh.warp.set1f ("uInject", p.inject);
    sh.warp.set1i ("uInjectMode", p.injectMode);
    sh.warp.set1f ("uPulse", p.beatPulse);
    sh.warp.set1f ("uKick", p.kick);
    sh.warp.set1i ("uWrap", wrap ? 1 : 0);
    gl::bindTex (0, buf[src].tex);
    gl::bindTex (1, p.inputTex);

    const auto& uv = state.meshUV();
    const int mw = milk::MilkState::meshW, mh = milk::MilkState::meshH;
    verts.clear();
    verts.reserve ((size_t) (mw * mh * 6 * 8));
    auto V = [&] (int gx, int gy)
    {
        const size_t i = (size_t) ((gy * (mw + 1) + gx) * 2);
        pushV (verts, (float) gx / mw * 2 - 1, (float) gy / mh * 2 - 1, uv[i], uv[i + 1], 1, 1, 1, 1);
    };
    for (int gy = 0; gy < mh; ++gy)
        for (int gx = 0; gx < mw; ++gx)
        {
            V (gx, gy); V (gx + 1, gy); V (gx + 1, gy + 1);
            V (gx, gy); V (gx + 1, gy + 1); V (gx, gy + 1);
        }
    geo.drawDynamic (verts, GL_TRIANGLES);

    // ---- 2. draw waves, shapes, borders, motion vectors on top
    sh.draw.use();
    sh.draw.set1i ("uTex", 0);
    sh.draw.set1i ("uTextured", 0);
    drawMotionVectors (geo, W, H);
    drawShapes (sh, geo, W, H, buf[src].tex);
    sh.draw.use();
    sh.draw.set1i ("uTextured", 0);
    drawWave (geo, state.mainWave, W, H);
    for (auto& w : state.waves)
        if (w.enabled) drawWave (geo, w, W, H);
    drawBorders (geo);
    glDisable (GL_BLEND);
    cur = dst;

    // ---- 3. composite (echo, gamma, filters)
    out.bind();
    sh.comp.use();
    sh.comp.set1i ("uState", 0);
    sh.comp.set1f ("uGamma", (float) juce::jlimit (0.0, 4.0, state.get ("gamma")));
    sh.comp.set1f ("uEchoZoom", (float) state.get ("echo_zoom"));
    sh.comp.set1f ("uEchoAlpha", (float) juce::jlimit (0.0, 1.0, state.get ("echo_alpha")));
    sh.comp.set1i ("uEchoOrient", (int) state.get ("echo_orient") % 4);
    sh.comp.set1i ("uDarkenCenter", state.get ("darken_center") != 0 ? 1 : 0);
    sh.comp.set1i ("uBrighten", state.get ("brighten") != 0 ? 1 : 0);
    sh.comp.set1i ("uDarken", state.get ("darken") != 0 ? 1 : 0);
    sh.comp.set1i ("uSolarize", state.get ("solarize") != 0 ? 1 : 0);
    sh.comp.set1i ("uInvert", state.get ("invert") != 0 ? 1 : 0);
    gl::bindTex (0, buf[cur].tex);
    verts.clear();
    pushV (verts, -1, -1, 0, 0, 1, 1, 1, 1); pushV (verts, 1, -1, 1, 0, 1, 1, 1, 1); pushV (verts, -1, 1, 0, 1, 1, 1, 1, 1);
    pushV (verts, 1, -1, 1, 0, 1, 1, 1, 1); pushV (verts, 1, 1, 1, 1, 1, 1, 1, 1); pushV (verts, -1, 1, 0, 1, 1, 1, 1, 1);
    geo.drawDynamic (verts, GL_TRIANGLES);
}

void MilkInstance::drawWave (gl::Geometry& geo, const milk::MilkState::WaveOut& w, int W, int H)
{
    if (! w.enabled || w.xy.size() < 4) return;
    verts.clear();
    const size_t n = w.xy.size() / 2;
    const float width = w.thick ? 3.0f : 1.6f;
    for (size_t i = 0; i < n; ++i)
    {
        const float* c = &w.rgba[i * 4];
        const float x = w.xy[i * 2], y = w.xy[i * 2 + 1];
        if (std::isnan (x)) continue;
        if (w.dots)
            appendDot (verts, x, y, w.thick ? 4.0f : 2.5f, W, H, c);
        else if (i + 1 < n && ! std::isnan (w.xy[(i + 1) * 2]))
            appendLine (verts, x, y, w.xy[(i + 1) * 2], w.xy[(i + 1) * 2 + 1], width, W, H, c, &w.rgba[(i + 1) * 4]);
    }
    setBlend (w.additive);
    geo.drawDynamic (verts, GL_TRIANGLES);
}

void MilkInstance::drawShapes (MilkShaders& sh, gl::Geometry& geo, int W, int H, GLuint prevTex)
{
    const float aspect = (float) H / (float) W;
    for (auto& s : state.shapes)
    {
        if (! s.enabled) continue;
        for (auto& in : s.inst)
        {
            const int n = in.sides;
            verts.clear();
            const float pi2 = juce::MathConstants<float>::twoPi;
            const float cx = in.x, cy = in.y;
            for (int k = 0; k < n; ++k)
            {
                const float a0 = in.ang + pi2 * (float) k / n + juce::MathConstants<float>::pi * 0.25f;
                const float a1 = in.ang + pi2 * (float) (k + 1) / n + juce::MathConstants<float>::pi * 0.25f;
                const float x0 = cx + in.rad * std::cos (a0) * aspect, y0 = cy + in.rad * std::sin (a0);
                const float x1 = cx + in.rad * std::cos (a1) * aspect, y1 = cy + in.rad * std::sin (a1);
                auto texUv = [&] (float a, float r)
                {
                    const float ta = a + in.texAng;
                    return juce::Point<float> (0.5f + r * std::cos (ta) / juce::jmax (0.01f, in.texZoom) * aspect,
                                               0.5f + r * std::sin (ta) / juce::jmax (0.01f, in.texZoom));
                };
                const auto t0 = texUv (a0, in.rad), t1 = texUv (a1, in.rad);
                pushV (verts, cx * 2 - 1, cy * 2 - 1, 0.5f, 0.5f, in.r, in.g, in.b, in.a);
                pushV (verts, x0 * 2 - 1, y0 * 2 - 1, t0.x, t0.y, in.r2, in.g2, in.b2, in.a2);
                pushV (verts, x1 * 2 - 1, y1 * 2 - 1, t1.x, t1.y, in.r2, in.g2, in.b2, in.a2);
            }
            sh.draw.use();
            sh.draw.set1i ("uTextured", in.textured ? 1 : 0);
            gl::bindTex (0, prevTex);
            setBlend (in.additive);
            if (in.a > 0.001f || in.a2 > 0.001f)
                geo.drawDynamic (verts, GL_TRIANGLES);

            if (in.ba > 0.001f)
            {
                verts.clear();
                const float bc[4] = { in.br, in.bg, in.bb, in.ba };
                for (int k = 0; k < n; ++k)
                {
                    const float a0 = in.ang + pi2 * (float) k / n + juce::MathConstants<float>::pi * 0.25f;
                    const float a1 = in.ang + pi2 * (float) (k + 1) / n + juce::MathConstants<float>::pi * 0.25f;
                    appendLine (verts, cx + in.rad * std::cos (a0) * aspect, cy + in.rad * std::sin (a0),
                                cx + in.rad * std::cos (a1) * aspect, cy + in.rad * std::sin (a1), in.thick ? 3.0f : 1.5f, W, H, bc, bc);
                }
                sh.draw.set1i ("uTextured", 0);
                setBlend (in.additive);
                geo.drawDynamic (verts, GL_TRIANGLES);
            }
        }
    }
}

void MilkInstance::drawBorders (gl::Geometry& geo)
{
    auto frame = [&] (float inset, float size, float r, float g, float b, float a)
    {
        if (a <= 0.001f || size <= 0.0f) return;
        verts.clear();
        auto rect = [&] (float x0, float y0, float x1, float y1)
        {
            x0 = x0 * 2 - 1; x1 = x1 * 2 - 1; y0 = y0 * 2 - 1; y1 = y1 * 2 - 1;
            pushV (verts, x0, y0, 0, 0, r, g, b, a); pushV (verts, x1, y0, 0, 0, r, g, b, a); pushV (verts, x1, y1, 0, 0, r, g, b, a);
            pushV (verts, x0, y0, 0, 0, r, g, b, a); pushV (verts, x1, y1, 0, 0, r, g, b, a); pushV (verts, x0, y1, 0, 0, r, g, b, a);
        };
        const float i0 = inset, i1 = inset + size;
        rect (i0, i0, 1 - i0, i1);
        rect (i0, 1 - i1, 1 - i0, 1 - i0);
        rect (i0, i1, i1, 1 - i1);
        rect (1 - i1, i1, 1 - i0, 1 - i1);
        setBlend (false);
        geo.drawDynamic (verts, GL_TRIANGLES);
    };
    const float ob = (float) state.get ("ob_size"), ib = (float) state.get ("ib_size");
    frame (0.0f, ob * 0.5f, (float) state.get ("ob_r"), (float) state.get ("ob_g"), (float) state.get ("ob_b"), (float) state.get ("ob_a"));
    frame (ob * 0.5f, ib * 0.5f, (float) state.get ("ib_r"), (float) state.get ("ib_g"), (float) state.get ("ib_b"), (float) state.get ("ib_a"));
}

void MilkInstance::drawMotionVectors (gl::Geometry& geo, int W, int H)
{
    const float a = (float) state.get ("mv_a");
    if (a <= 0.001f) return;
    const int nx = juce::jlimit (1, 64, (int) state.get ("mv_x")), ny = juce::jlimit (1, 48, (int) state.get ("mv_y"));
    const float c[4] = { (float) state.get ("mv_r"), (float) state.get ("mv_g"), (float) state.get ("mv_b"), a };
    const float len = (float) state.get ("mv_l");
    const auto& uv = state.meshUV();
    const int mw = milk::MilkState::meshW, mh = milk::MilkState::meshH;
    verts.clear();
    for (int j = 0; j < ny; ++j)
        for (int i = 0; i < nx; ++i)
        {
            const float x = ((float) i + 0.5f) / nx + (float) state.get ("mv_dx");
            const float y = ((float) j + 0.5f) / ny + (float) state.get ("mv_dy");
            const int gx = juce::jlimit (0, mw, (int) std::round (x * mw)), gy = juce::jlimit (0, mh, (int) std::round (y * mh));
            const size_t k = (size_t) ((gy * (mw + 1) + gx) * 2);
            const float dx = (x - uv[k]) * len * 6.0f, dy = (y - uv[k + 1]) * len * 6.0f;
            const float lx = std::abs (dx) + std::abs (dy) < 0.002f ? 0.002f : dx;
            appendLine (verts, x, y, x + lx, y + dy, 1.5f, W, H, c, c);
        }
    setBlend (false);
    geo.drawDynamic (verts, GL_TRIANGLES);
}

//==============================================================================
void MilkSource::setPreset (const juce::String& newSpec, float blendSeconds)
{
    if (newSpec == spec && cur != nullptr) return;
    pendingSpec = newSpec;
    blendTime = blendSeconds;
}

void MilkSource::render (MilkShaders& sh, gl::Geometry& geo, const MilkFrameParams& p, gl::Target& dest)
{
    now = p.in.time;
    if (pendingSpec.isNotEmpty() && pendingSpec != spec)
    {
        milk::MilkPresetData data;
        if (pendingSpec.startsWith ("builtin:"))
        {
            const auto nm = pendingSpec.fromFirstOccurrenceOf ("builtin:", false, false);
            data = milk::MilkPresetData::parse (milk::BuiltinMilkPresets::text (nm), nm);
        }
        else
            data = milk::MilkPresetData::load (juce::File (pendingSpec));

        auto inst = std::make_unique<MilkInstance>();
        juce::String errs;
        inst->load (data, errs);
        lastErrors = errs;
        hasPixelShaders = data.hasPixelShaders;
        if (cur != nullptr)
        {
            inst->seedFrom (sh, geo, *cur);
            prev = std::move (cur);
            blendStart = now;
        }
        cur = std::move (inst);
        spec = pendingSpec;
        pendingSpec.clear();
    }
    if (cur == nullptr) { dest.clear(); return; }

    cur->render (sh, geo, p);
    float t = 1.0f;
    if (prev != nullptr)
    {
        t = blendTime > 0.0 ? (float) juce::jlimit (0.0, 1.0, (now - blendStart) / blendTime) : 1.0f;
        if (t >= 1.0f) prev.reset();
        else prev->render (sh, geo, p);
    }
    dest.bind();
    glDisable (GL_BLEND);
    sh.copy.use();
    sh.copy.set1i ("uA", 0);
    sh.copy.set1i ("uB", 1);
    sh.copy.set1f ("uMix", t);
    gl::bindTex (0, prev != nullptr ? prev->output() : cur->output());
    gl::bindTex (1, cur->output());
    std::vector<float> v;
    pushV (v, -1, -1, 0, 0, 1, 1, 1, 1); pushV (v, 1, -1, 1, 0, 1, 1, 1, 1); pushV (v, -1, 1, 0, 1, 1, 1, 1, 1);
    pushV (v, 1, -1, 1, 0, 1, 1, 1, 1); pushV (v, 1, 1, 1, 1, 1, 1, 1, 1); pushV (v, -1, 1, 0, 1, 1, 1, 1, 1);
    geo.drawDynamic (v, GL_TRIANGLES);
}
} // namespace moshdeck
