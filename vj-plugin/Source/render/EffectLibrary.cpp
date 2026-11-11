#include "EffectLibrary.h"

namespace moshdeck
{
std::vector<ParamDef> makeParams (std::initializer_list<P> list)
{
    std::vector<ParamDef> out;
    int i = 0;
    for (auto& p : list)
        out.push_back ({ "p" + juce::String (i++), p.name, p.mn, p.mx, p.def });
    return out;
}

static ShaderDef E (const char* id, const char* name, const char* cat, std::initializer_list<P> params,
                    const char* glsl, bool history = false, const char* desc = "")
{
    ShaderDef d;
    d.id = id; d.name = name; d.category = cat; d.description = desc;
    d.params = makeParams (params);
    d.glsl = glsl;
    d.usesHistory = history;
    return d;
}

#define REFRESH_P { "Refresh (0 off,1 beat,2 bar,3 kick)", 0, 3, 1 }

static std::vector<ShaderDef> buildEffects()
{
    std::vector<ShaderDef> v;

    //==========================================================================
    // DATAMOSH - real-time emulations of codec artifacts: P-frame motion
    // compensation without I-frames, macroblock smear, residual drift, etc.
    //==========================================================================
    v.push_back (E ("mosh_bloom", "Pixel Bloom", "Datamosh",
        { { "Motion Gain", 0, 6, 1.6f }, { "Residual", 0, 1, 0.12f }, { "Block Size", 1, 64, 16 }, REFRESH_P, { "Leak To Input", 0, 0.2f, 0.0f } },
        R"GLSL(
vec4 fx(vec2 uv) {
    vec4 cur = texture(uIn, uv);
    if (refreshNow(P3)) return cur;
    vec2 b = blockUv(uv, P2);
    vec2 f = flowAt(b, max(P2 * 0.5, 2.0)) * P0;
    vec4 moved = texture(uPrev, uv - f);
    vec4 resid = cur - texture(uPrevIn, uv);
    return clamp(mix(moved + resid * P1, cur, P4), 0.0, 1.0);
})GLSL", true, "Classic datamosh: the last frame keeps being pushed around by the motion of the new frames."));

    v.push_back (E ("mosh_melt", "Mosh Melt", "Datamosh",
        { { "Motion Gain", 0, 8, 2.5f }, { "Residual", 0, 1, 0.05f }, REFRESH_P, { "Drift X", -0.01f, 0.01f, 0 }, { "Drift Y", -0.01f, 0.01f, 0.001f } },
        R"GLSL(
vec4 fx(vec2 uv) {
    vec4 cur = texture(uIn, uv);
    if (refreshNow(P2)) return cur;
    vec2 f = flowAt(uv, 3.0) * P0 + vec2(P3, P4) * (0.5 + uBass);
    vec4 moved = texture(uPrev, uv - f);
    return clamp(moved + (cur - texture(uPrevIn, uv)) * P1, 0.0, 1.0);
})GLSL", true));

    v.push_back (E ("mosh_iframe", "I-Frame Drop", "Datamosh",
        { { "Residual Strength", 0, 2, 1 }, REFRESH_P, { "Shift Amount", 0, 0.02f, 0.0f } },
        R"GLSL(
vec4 fx(vec2 uv) {
    vec4 cur = texture(uIn, uv);
    if (refreshNow(P1)) return cur;
    vec2 off = (hash22(floor(uv * 12.0) + floor(uBeats)) - 0.5) * P2;
    vec4 diff = cur - texture(uPrevIn, uv);
    return clamp(texture(uPrev, uv + off) + diff * P0, 0.0, 1.0);
})GLSL", true, "Only the difference between frames is applied, like a stream that lost its keyframes."));

    v.push_back (E ("mosh_blockfreeze", "Block Freeze", "Datamosh",
        { { "Block Size", 4, 96, 24 }, { "Freeze Amount", 0, 1, 0.4f }, { "Reseed (beats)", 0.25f, 8, 1 }, REFRESH_P },
        R"GLSL(
vec4 fx(vec2 uv) {
    vec4 cur = texture(uIn, uv);
    if (refreshNow(P3)) return cur;
    vec2 cell = floor(uv * uRes / P0);
    float seed = floor(uBeats / max(P2, 0.01));
    bool frozen = hash21(cell + seed * 13.7) < P1;
    return frozen ? texture(uPrev, uv) : cur;
})GLSL", true));

    v.push_back (E ("mosh_macroshift", "Macroblock Shift", "Datamosh",
        { { "Block Size", 4, 64, 16 }, { "Gain", 0, 12, 4 }, REFRESH_P, { "Residual", 0, 1, 0.1f } },
        R"GLSL(
vec4 fx(vec2 uv) {
    vec4 cur = texture(uIn, uv);
    if (refreshNow(P2)) return cur;
    vec2 b = blockUv(uv, P0);
    vec2 f = flowAt(b, P0 * 0.5) * P1;
    vec2 q = floor(f * uRes / P0 * 2.0 + 0.5) * P0 * 0.5 / uRes;
    return clamp(texture(uPrev, uv - q) + (cur - texture(uPrevIn, uv)) * P3, 0.0, 1.0);
})GLSL", true));

    v.push_back (E ("mosh_transfer", "Motion Transfer", "Datamosh",
        { { "Motion Gain", 0, 8, 2 }, { "Residual", 0, 1, 0.0f }, { "Block Size", 1, 64, 8 }, REFRESH_P, { "Motion From (0 B, 1 A)", 0, 1, 0 } },
        R"GLSL(
vec4 fx(vec2 uv) {
    vec4 cur = texture(uIn, uv);
    if (refreshNow(P3)) return cur;
    vec2 b = blockUv(uv, P2);
    vec2 f = (P4 < 0.5 ? flowBetween(uDeckB, uDeckBPrev, b, max(P2 * 0.5, 2.0))
                       : flowBetween(uDeckA, uDeckAPrev, b, max(P2 * 0.5, 2.0))) * P0;
    return clamp(texture(uPrev, uv - f) + (cur - texture(uPrevIn, uv)) * P1, 0.0, 1.0);
})GLSL", true, "Moves this picture with the motion of the other deck - the classic two-clip datamosh."));

    v.push_back (E ("mosh_keybleed", "Keyframe Bleed", "Datamosh",
        { { "Change Threshold", 0, 0.5f, 0.08f }, { "Hold", 0, 1, 0.92f }, REFRESH_P },
        R"GLSL(
vec4 fx(vec2 uv) {
    vec4 cur = texture(uIn, uv);
    if (refreshNow(P2)) return cur;
    float d = length(cur.rgb - texture(uPrevIn, uv).rgb);
    return d > P0 ? cur : mix(cur, texture(uPrev, uv), P1);
})GLSL", true, "Pixels only update where the picture changes a lot; still areas stick."));

    v.push_back (E ("mosh_dct", "DCT Crush", "Datamosh",
        { { "Block Size", 4, 32, 8 }, { "Quality", 1, 32, 6 }, { "Chroma Crush", 0, 1, 0.5f }, { "Audio Crush", 0, 1, 0.3f } },
        R"GLSL(
vec4 fx(vec2 uv) {
    vec2 bs = P0 / uRes;
    vec2 b0 = floor(uv / bs) * bs;
    vec2 local = (uv - b0) / bs;
    vec3 m = vec3(0), gx = vec3(0), gy = vec3(0);
    for (int y = 0; y < 3; y++) for (int x = 0; x < 3; x++) {
        vec2 t = (vec2(x, y) + 0.5) / 3.0;
        vec3 c = texture(uIn, b0 + t * bs).rgb;
        m += c; gx += c * (t.x - 0.5); gy += c * (t.y - 0.5);
    }
    m /= 9.0; gx *= 2.0 / 3.0; gy *= 2.0 / 3.0;
    float q = max(1.0, P1 * (1.0 - P3 * uBass * 0.8));
    vec3 col = floor((m + gx * (local.x - 0.5) * 2.0 + gy * (local.y - 0.5) * 2.0) * q + 0.5) / q;
    float Y = luma(col);
    vec3 chroma = col - Y;
    chroma = floor(chroma * q * (1.0 - P2 * 0.8) + 0.5) / max(q * (1.0 - P2 * 0.8), 1.0);
    return vec4(clamp(Y + chroma, 0.0, 1.0), 1.0);
})GLSL"));

    v.push_back (E ("mosh_chroma", "Chroma Bleed", "Datamosh",
        { { "Bleed (px)", 0, 64, 12 }, { "Smear Feedback", 0, 0.98f, 0.6f }, { "Subsample", 1, 16, 4 } },
        R"GLSL(
vec4 fx(vec2 uv) {
    vec3 cur = texture(uIn, uv).rgb;
    float Y = luma(cur);
    vec2 suv = vec2(floor(uv.x * uRes.x / P2) * P2 / uRes.x, uv.y);
    vec3 c = texture(uIn, suv - vec2(P0 / uRes.x, 0)).rgb;
    vec3 prev = texture(uPrev, uv - vec2(1.5 / uRes.x, 0)).rgb;
    vec3 chroma = mix(c - luma(c), prev - luma(prev), P1);
    return vec4(clamp(Y + chroma, 0.0, 1.0), 1.0);
})GLSL", true));

    v.push_back (E ("mosh_vectors", "Motion Vector View", "Datamosh",
        { { "Gain", 0, 40, 12 }, { "Mix With Input", 0, 1, 0.5f }, { "Block Size", 1, 64, 12 } },
        R"GLSL(
vec4 fx(vec2 uv) {
    vec2 f = flowAt(blockUv(uv, P2), max(P2 * 0.5, 2.0)) * uRes * P0 * 0.05;
    vec3 col = hsv2rgb(vec3(atan(f.y, f.x) / TAU + 0.5, 1.0, clamp(length(f), 0.0, 1.0)));
    return vec4(mix(col, texture(uIn, uv).rgb, P1 * (1.0 - clamp(length(f), 0.0, 1.0))), 1.0);
})GLSL", true));

    v.push_back (E ("mosh_pixelsort", "Pixel Sort", "Datamosh",
        { { "Low Threshold", 0, 1, 0.25f }, { "Refresh Mix", 0, 1, 0.04f }, { "Vertical", 0, 1, 1 }, { "High Threshold", 0, 1, 1 }, { "Reverse", 0, 1, 0 } },
        R"GLSL(
vec4 fx(vec2 uv) {
    vec4 cur = texture(uIn, uv);
    if (uLocalFrame < 2) return cur;
    bool vert = P2 > 0.5;
    vec2 axis = vert ? vec2(0.0, 1.0 / uRes.y) : vec2(1.0 / uRes.x, 0.0);
    float coord = floor(vert ? uv.y * uRes.y : uv.x * uRes.x);
    bool even = mod(coord + float(uFrame), 2.0) < 1.0;
    vec2 nuv = uv + (even ? axis : -axis);
    vec4 a = mix(texture(uPrev, uv), cur, P1);
    vec4 b = mix(texture(uPrev, nuv), texture(uIn, nuv), P1);
    float la = luma(a.rgb), lb = luma(b.rgb);
    bool inRange = la > P0 && lb > P0 && la < P3 + 0.001 && lb < P3 + 0.001;
    bool asc = P4 < 0.5;
    bool swp = inRange && (even ? (asc ? la > lb : la < lb) : (asc ? la < lb : la > lb));
    return swp ? b : a;
})GLSL", true, "Genuine odd-even transposition pixel sorting that keeps sorting over time."));

    v.push_back (E ("mosh_slices", "Glitch Slices", "Datamosh",
        { { "Intensity", 0, 0.5f, 0.08f }, { "Slices", 2, 80, 24 }, { "Rate (per beat)", 0.25f, 16, 2 }, { "RGB Split", 0, 1, 0.5f }, { "Kick Boost", 0, 1, 0.6f } },
        R"GLSL(
vec4 fx(vec2 uv) {
    float t = floor(uBeats * P2);
    float row = floor(uv.y * P1);
    float h = hash21(vec2(row, t));
    float amt = P0 * (1.0 + uKick * P4 * 3.0);
    float off = h > 0.6 ? (hash21(vec2(row, t + 7.0)) - 0.5) * amt : 0.0;
    float s = off * P3 * 0.5;
    return vec4(texture(uIn, uv + vec2(off + s, 0)).r, texture(uIn, uv + vec2(off, 0)).g, texture(uIn, uv + vec2(off - s, 0)).b, 1.0);
})GLSL"));

    v.push_back (E ("mosh_blockdisplace", "Block Displace", "Datamosh",
        { { "Amount", 0, 0.3f, 0.05f }, { "Block Size", 4, 128, 32 }, { "Probability", 0, 1, 0.3f }, { "Audio React", 0, 1, 0.7f } },
        R"GLSL(
vec4 fx(vec2 uv) {
    vec2 cell = floor(uv * uRes / P1);
    float t = floor(uBeats * 4.0);
    float p = P2 * (1.0 - P3 + P3 * (uBass + uKick));
    vec2 off = hash21(cell + t) < p ? (hash22(cell * 1.3 + t) - 0.5) * P0 * 2.0 : vec2(0);
    return texture(uIn, uv + off);
})GLSL"));

    v.push_back (E ("mosh_corrupt", "Data Corrupt", "Datamosh",
        { { "Amount", 0, 1, 0.25f }, { "Block Size", 2, 64, 16 }, { "Colour Noise", 0, 1, 0.5f }, { "Rate", 0.25f, 16, 4 } },
        R"GLSL(
vec4 fx(vec2 uv) {
    vec2 cell = floor(uv * uRes / vec2(P1 * 4.0, P1));
    float t = floor(uBeats * P3);
    vec4 c = texture(uIn, uv);
    float h = hash21(cell + t * 3.1);
    if (h < P0 * (0.4 + uLevel)) {
        int mode = int(hash21(cell * 2.7 + t) * 4.0);
        if (mode == 0) c.rgb = c.gbr;
        else if (mode == 1) c.rgb = vec3(1.0) - c.rgb;
        else if (mode == 2) c.rgb = floor(c.rgb * 3.0) / 3.0 + vec3(hash21(cell), hash21(cell + 1.0), hash21(cell + 2.0)) * P2;
        else c = texture(uIn, vec2(uv.x, floor(uv.y * 8.0) / 8.0));
    }
    return vec4(clamp(c.rgb, 0.0, 1.0), 1.0);
})GLSL"));

    v.push_back (E ("mosh_smear", "Directional Smear", "Datamosh",
        { { "Length", 0, 0.99f, 0.9f }, { "Angle", 0, 6.283f, 4.71f }, { "Threshold", 0, 1, 0.4f }, { "Step (px)", 0, 8, 2 } },
        R"GLSL(
vec4 fx(vec2 uv) {
    vec4 cur = texture(uIn, uv);
    if (uLocalFrame < 2) return cur;
    vec2 dir = vec2(cos(P1), sin(P1)) * P3 / uRes;
    vec4 prev = texture(uPrev, uv - dir);
    float k = luma(prev.rgb) > P2 ? P0 : 0.0;
    return max(cur, prev * k);
})GLSL", true));

    v.push_back (E ("mosh_ghost", "Ghost Trails", "Datamosh",
        { { "Feedback", 0, 0.99f, 0.85f }, { "Difference Boost", 0, 4, 1 }, { "Hue Drift", 0, 0.1f, 0.01f } },
        R"GLSL(
vec4 fx(vec2 uv) {
    vec4 cur = texture(uIn, uv);
    vec3 prev = texture(uPrev, uv).rgb;
    vec3 hsv = rgb2hsv(prev); hsv.x = fract(hsv.x + P2); prev = hsv2rgb(hsv);
    vec3 d = abs(cur.rgb - texture(uPrevIn, uv).rgb) * P1;
    return vec4(clamp(max(cur.rgb, prev * P0) + d * 0.5, 0.0, 1.0), 1.0);
})GLSL", true));

    v.push_back (E ("mosh_tear", "Scanline Tear", "Datamosh",
        { { "Amount", 0, 0.2f, 0.03f }, { "Frequency", 1, 200, 40 }, { "Speed", 0, 10, 2 }, { "Kick Boost", 0, 1, 0.5f } },
        R"GLSL(
vec4 fx(vec2 uv) {
    float n = noise(vec2(uv.y * P1, uTime * P2));
    float off = (n - 0.5) * P0 * (1.0 + uKick * P3 * 4.0) * step(0.55, n);
    return texture(uIn, vec2(fract(uv.x + off), uv.y));
})GLSL"));

    v.push_back (E ("mosh_vhs", "Bad Signal", "Datamosh",
        { { "Noise", 0, 1, 0.2f }, { "Wobble", 0, 0.02f, 0.004f }, { "Chroma Offset", 0, 0.02f, 0.004f }, { "Tracking Roll", 0, 1, 0.2f } },
        R"GLSL(
vec4 fx(vec2 uv) {
    float roll = fract(uTime * 0.13) ;
    float band = smoothstep(0.0, 0.05, abs(uv.y - roll)) ;
    vec2 p = uv;
    p.x += sin(uv.y * 80.0 + uTime * 6.0) * P1 + (1.0 - band) * P3 * 0.05 * (hash11(uTime) - 0.5);
    float r = texture(uIn, p + vec2(P2, 0)).r;
    float g = texture(uIn, p).g;
    float b = texture(uIn, p - vec2(P2, 0)).b;
    float n = hash21(uv * uRes + uTime * 100.0);
    vec3 col = vec3(r, g, b) * (0.9 + 0.1 * sin(uv.y * uRes.y * 1.5));
    col = mix(col, vec3(n), P0 * (0.3 + 0.7 * (1.0 - band)) * 0.6);
    return vec4(col, 1.0);
})GLSL"));

    v.push_back (E ("mosh_interlace", "Interlace Glitch", "Datamosh",
        { { "Field Offset", 0, 0.05f, 0.01f }, { "Line Size", 1, 16, 2 }, { "Field Hold", 0, 1, 0.5f } },
        R"GLSL(
vec4 fx(vec2 uv) {
    float line = floor(uv.y * uRes.y / P1);
    bool odd = mod(line, 2.0) > 0.5;
    if (!odd) return texture(uIn, uv);
    vec4 shifted = texture(uIn, uv + vec2(P0 * (0.5 + uBass), 0));
    return mix(shifted, texture(uPrev, uv), P2);
})GLSL", true));

    v.push_back (E ("mosh_bitcrush", "Bit Crush", "Datamosh",
        { { "Bits", 1, 8, 3 }, { "Pixelate (px)", 1, 32, 1 }, { "Dither", 0, 1, 0.5f } },
        R"GLSL(
vec4 fx(vec2 uv) {
    vec2 b = blockUv(uv, P1);
    vec3 c = texture(uIn, b).rgb;
    float levels = pow(2.0, floor(P0)) - 1.0;
    float d = (hash21(floor(uv * uRes)) - 0.5) * P2 / levels;
    return vec4(floor((c + d) * levels + 0.5) / levels, 1.0);
})GLSL"));

    v.push_back (E ("mosh_quantmotion", "Quantized Motion", "Datamosh",
        { { "Block Size", 4, 64, 16 }, { "Gain", 0, 10, 3 }, REFRESH_P },
        R"GLSL(
vec4 fx(vec2 uv) {
    vec4 cur = texture(uIn, uv);
    if (refreshNow(P2)) return cur;
    vec2 f = flowAt(blockUv(uv, P0), P0 * 0.5) * P1;
    vec2 q = floor(f * uRes + 0.5) / uRes;
    vec2 local = fract(uv * uRes / P0);
    vec4 moved = texture(uPrev, uv - q);
    float edge = step(0.94, max(local.x, local.y));
    return mix(moved, moved * 0.85, edge * step(0.0005, length(q)));
})GLSL", true));

    v.push_back (E ("mosh_drift", "Codec Drift", "Datamosh",
        { { "Motion Gain", 0, 6, 2 }, { "Hue Drift", 0, 0.2f, 0.03f }, REFRESH_P, { "Saturation Boost", 0, 0.2f, 0.02f } },
        R"GLSL(
vec4 fx(vec2 uv) {
    vec4 cur = texture(uIn, uv);
    if (refreshNow(P2)) return cur;
    vec2 f = flowAt(uv, 4.0) * P0;
    vec3 c = texture(uPrev, uv - f).rgb;
    vec3 h = rgb2hsv(c);
    float m = clamp(length(f) * 200.0, 0.0, 1.0);
    h.x = fract(h.x + P1 * m); h.y = clamp(h.y + P3 * m, 0.0, 1.0);
    return vec4(hsv2rgb(h), 1.0);
})GLSL", true));

    v.push_back (E ("mosh_stutter", "Frame Stutter", "Datamosh",
        { { "Hold Probability", 0, 1, 0.4f }, { "Grid (per beat)", 0.25f, 16, 2 } },
        R"GLSL(
vec4 fx(vec2 uv) {
    float slot = floor(uBeats * P1);
    bool hold = hash11(slot * 1.37) < P0 && uLocalFrame > 2;
    return hold ? texture(uPrev, uv) : texture(uIn, uv);
})GLSL", true));

    v.push_back (E ("mosh_wave", "Wave Mosh", "Datamosh",
        { { "Amplitude", 0, 0.02f, 0.004f }, { "Frequency", 1, 60, 12 }, REFRESH_P, { "Residual", 0, 1, 0.2f } },
        R"GLSL(
vec4 fx(vec2 uv) {
    vec4 cur = texture(uIn, uv);
    if (refreshNow(P2)) return cur;
    vec2 d = vec2(sin(uv.y * P1 + uTime * 2.0), cos(uv.x * P1 * 0.8 + uTime * 1.7)) * P0 * (0.5 + uBass * 1.5);
    return clamp(texture(uPrev, uv + d) + (cur - texture(uPrevIn, uv)) * P3, 0.0, 1.0);
})GLSL", true));

    v.push_back (E ("mosh_rgbflow", "RGB Flow Split", "Datamosh",
        { { "Motion Gain", 0, 8, 2 }, { "Channel Spread", 0, 2, 0.6f }, REFRESH_P },
        R"GLSL(
vec4 fx(vec2 uv) {
    vec4 cur = texture(uIn, uv);
    if (refreshNow(P2)) return cur;
    vec2 f = flowAt(uv, 4.0) * P0;
    float r = texture(uPrev, uv - f * (1.0 + P1)).r;
    float g = texture(uPrev, uv - f).g;
    float b = texture(uPrev, uv - f * (1.0 - P1)).b;
    vec3 resid = (cur - texture(uPrevIn, uv)).rgb * 0.15;
    return vec4(clamp(vec3(r, g, b) + resid, 0.0, 1.0), 1.0);
})GLSL", true));

    v.push_back (E ("mosh_jpeg", "JPEG Ringing", "Datamosh",
        { { "Ringing", 0, 4, 1.5f }, { "Block Size", 4, 32, 8 }, { "Block Edges", 0, 1, 0.3f } },
        R"GLSL(
vec4 fx(vec2 uv) {
    vec2 px = 1.0 / uRes;
    vec3 c = texture(uIn, uv).rgb;
    vec3 blur = (texture(uIn, uv + vec2(px.x * 2.0, 0)).rgb + texture(uIn, uv - vec2(px.x * 2.0, 0)).rgb +
                 texture(uIn, uv + vec2(0, px.y * 2.0)).rgb + texture(uIn, uv - vec2(0, px.y * 2.0)).rgb) * 0.25;
    vec3 ring = c + (c - blur) * P0 * (0.5 + 0.5 * sin(dot(uv * uRes, vec2(1.7, 1.3))));
    vec2 l = fract(uv * uRes / P1);
    float e = (step(l.x, 1.0 / P1) + step(l.y, 1.0 / P1)) * P2 * 0.15;
    return vec4(clamp(ring - e, 0.0, 1.0), 1.0);
})GLSL"));

    v.push_back (E ("mosh_static", "Static Mosh", "Datamosh",
        { { "Noise Amount", 0, 1, 0.3f }, { "Block Size", 1, 64, 8 }, { "Feedback", 0, 0.99f, 0.7f } },
        R"GLSL(
vec4 fx(vec2 uv) {
    vec4 cur = texture(uIn, uv);
    vec2 cell = floor(uv * uRes / P1);
    float n = hash21(cell + floor(uTime * 30.0));
    vec3 prev = texture(uPrev, uv + (hash22(cell + uTime) - 0.5) * 0.01).rgb;
    vec3 col = mix(cur.rgb, prev, P2 * step(0.5, n));
    col = mix(col, vec3(n), P0 * uHat);
    return vec4(col, 1.0);
})GLSL", true));

    v.push_back (E ("mosh_swap", "Deck Block Swap", "Datamosh",
        { { "Swap Probability", 0, 1, 0.3f }, { "Block Size", 4, 128, 32 }, { "Rate (per beat)", 0.25f, 16, 1 }, { "Kick Boost", 0, 1, 0.5f } },
        R"GLSL(
vec4 fx(vec2 uv) {
    vec2 cell = floor(uv * uRes / P1);
    float t = floor(uBeats * P2);
    bool sw = hash21(cell + t * 5.3) < P0 + uKick * P3 * 0.5;
    return sw ? texture(uDeckB, uv) : texture(uIn, uv);
})GLSL"));

    v.push_back (E ("mosh_gravity", "Gravity Melt", "Datamosh",
        { { "Fall Speed (px)", 0, 8, 2 }, { "Threshold", 0, 1, 0.5f }, REFRESH_P, { "Hold", 0, 1, 0.97f } },
        R"GLSL(
vec4 fx(vec2 uv) {
    vec4 cur = texture(uIn, uv);
    if (refreshNow(P2)) return cur;
    vec4 above = texture(uPrev, uv + vec2(0, P0 / uRes.y));
    vec4 self = texture(uPrev, uv);
    vec4 r = luma(above.rgb) > P1 ? max(self, above) : self;
    return mix(cur, r, P3);
})GLSL", true));

    //==========================================================================
    // COLOR
    //==========================================================================
    v.push_back (E ("hue_shift", "Hue Shift", "Color", { { "Shift", 0, 1, 0.1f }, { "Beat Rotate", 0, 1, 0 } },
        R"GLSL(
vec4 fx(vec2 uv) { vec4 c = texture(uIn, uv); vec3 h = rgb2hsv(c.rgb); h.x = fract(h.x + P0 + uBeats * P1 * 0.25); return vec4(hsv2rgb(h), c.a); })GLSL"));
    v.push_back (E ("saturation", "Saturation", "Color", { { "Saturation", 0, 3, 1.5f } },
        R"GLSL(
vec4 fx(vec2 uv) { vec4 c = texture(uIn, uv); return vec4(mix(vec3(luma(c.rgb)), c.rgb, P0), c.a); })GLSL"));
    v.push_back (E ("invert", "Invert", "Color", { { "Amount", 0, 1, 1 }, { "On Kick", 0, 1, 0 } },
        R"GLSL(
vec4 fx(vec2 uv) { vec4 c = texture(uIn, uv); float a = P1 > 0.5 ? step(0.5, uKick) : P0; return vec4(mix(c.rgb, 1.0 - c.rgb, a), c.a); })GLSL"));
    v.push_back (E ("posterize", "Posterize", "Color", { { "Levels", 2, 16, 4 }, { "Gamma", 0.3f, 3, 1 } },
        R"GLSL(
vec4 fx(vec2 uv) { vec3 c = pow(texture(uIn, uv).rgb, vec3(P1)); c = floor(c * P0) / (P0 - 1.0); return vec4(pow(c, vec3(1.0 / P1)), 1.0); })GLSL"));
    v.push_back (E ("gradient_map", "Gradient Map", "Color", { { "Hue A", 0, 1, 0.75f }, { "Hue B", 0, 1, 0.1f }, { "Cycle Speed", 0, 1, 0.1f }, { "Contrast", 0.2f, 4, 1.2f } },
        R"GLSL(
vec4 fx(vec2 uv) { float l = pow(luma(texture(uIn, uv).rgb), P3); float s = uTime * P2;
  vec3 a = hsv2rgb(vec3(fract(P0 + s), 0.9, 0.2)), b = hsv2rgb(vec3(fract(P1 + s), 0.8, 1.0));
  return vec4(mix(a, b, l) * (0.3 + l), 1.0); })GLSL"));
    v.push_back (E ("levels", "Levels", "Color", { { "Brightness", -1, 1, 0 }, { "Contrast", 0, 3, 1 }, { "Gamma", 0.2f, 3, 1 } },
        R"GLSL(
vec4 fx(vec2 uv) { vec3 c = texture(uIn, uv).rgb; c = (c - 0.5) * P1 + 0.5 + P0; return vec4(pow(clamp(c, 0.0, 1.0), vec3(1.0 / P2)), 1.0); })GLSL"));
    v.push_back (E ("threshold", "Threshold", "Color", { { "Threshold", 0, 1, 0.5f }, { "Softness", 0, 0.5f, 0.05f }, { "Audio Shift", 0, 1, 0.3f } },
        R"GLSL(
vec4 fx(vec2 uv) { float t = P0 - uBass * P2 * 0.4; float l = luma(texture(uIn, uv).rgb); return vec4(vec3(smoothstep(t - P1, t + P1, l)), 1.0); })GLSL"));
    v.push_back (E ("duotone", "Duotone", "Color", { { "Shadow Hue", 0, 1, 0.65f }, { "Highlight Hue", 0, 1, 0.95f }, { "Saturation", 0, 1, 0.8f } },
        R"GLSL(
vec4 fx(vec2 uv) { float l = luma(texture(uIn, uv).rgb); return vec4(mix(hsv2rgb(vec3(P0, P2, 0.15)), hsv2rgb(vec3(P1, P2, 1.0)), l), 1.0); })GLSL"));
    v.push_back (E ("color_cycle", "Color Cycle", "Color", { { "Speed", 0, 4, 0.5f }, { "Bands", 1, 12, 3 }, { "Mix", 0, 1, 0.7f } },
        R"GLSL(
vec4 fx(vec2 uv) { vec3 c = texture(uIn, uv).rgb; vec3 r = rainbow(luma(c) * P1 + uTime * P0); return vec4(mix(c, r * (0.2 + luma(c)), P2), 1.0); })GLSL"));
    v.push_back (E ("rgb_gain", "RGB Gain", "Color", { { "Red", 0, 2, 1 }, { "Green", 0, 2, 1 }, { "Blue", 0, 2, 1 } },
        R"GLSL(
vec4 fx(vec2 uv) { vec4 c = texture(uIn, uv); return vec4(c.rgb * vec3(P0, P1, P2), c.a); })GLSL"));
    v.push_back (E ("solarize", "Solarize", "Color", { { "Threshold", 0, 1, 0.5f } },
        R"GLSL(
vec4 fx(vec2 uv) { vec3 c = texture(uIn, uv).rgb; return vec4(mix(c, 1.0 - c, step(P0, c)), 1.0); })GLSL"));
    v.push_back (E ("channel_swap", "Channel Swap", "Color", { { "Mode (0-5)", 0, 5, 1 } },
        R"GLSL(
vec4 fx(vec2 uv) { vec3 c = texture(uIn, uv).rgb; int m = int(P0 + 0.5);
  if (m == 1) c = c.gbr; else if (m == 2) c = c.brg; else if (m == 3) c = c.rbg; else if (m == 4) c = c.bgr; else if (m == 5) c = c.grb;
  return vec4(c, 1.0); })GLSL"));

    //==========================================================================
    // DISTORT
    //==========================================================================
    v.push_back (E ("kaleido", "Kaleidoscope", "Distort", { { "Segments", 2, 24, 6 }, { "Rotation Speed", -2, 2, 0.2f }, { "Zoom", 0.2f, 4, 1 }, { "Bass Zoom", 0, 1, 0.2f } },
        R"GLSL(
vec4 fx(vec2 uv) {
    vec2 p = aspectUv(uv) / (P2 * (1.0 + uBass * P3));
    float a = atan(p.y, p.x) + uTime * P1, r = length(p);
    float seg = TAU / floor(P0);
    a = mod(a, seg); a = abs(a - seg * 0.5);
    p = vec2(cos(a), sin(a)) * r;
    return texture(uIn, fract(p * vec2(uRes.y / uRes.x, 1.0) + 0.5));
})GLSL"));
    v.push_back (E ("mirror", "Mirror", "Distort", { { "Mode (0 H,1 V,2 Quad)", 0, 2, 0 } },
        R"GLSL(
vec4 fx(vec2 uv) { int m = int(P0 + 0.5); vec2 p = uv;
  if (m == 0 || m == 2) p.x = p.x > 0.5 ? 1.0 - p.x : p.x;
  if (m == 1 || m == 2) p.y = p.y > 0.5 ? 1.0 - p.y : p.y;
  return texture(uIn, p); })GLSL"));
    v.push_back (E ("twirl", "Twirl", "Distort", { { "Strength", -10, 10, 3 }, { "Radius", 0.05f, 1.5f, 0.6f }, { "Audio React", 0, 1, 0.5f } },
        R"GLSL(
vec4 fx(vec2 uv) { vec2 p = aspectUv(uv); float r = length(p); float k = max(0.0, 1.0 - r / P1);
  p = rot2(k * k * P0 * (1.0 + uBass * P2)) * p;
  return texture(uIn, p * vec2(uRes.y / uRes.x, 1.0) + 0.5); })GLSL"));
    v.push_back (E ("ripple", "Ripple", "Distort", { { "Amplitude", 0, 0.1f, 0.02f }, { "Frequency", 1, 80, 25 }, { "Speed", 0, 20, 4 }, { "Kick Boost", 0, 1, 0.6f } },
        R"GLSL(
vec4 fx(vec2 uv) { vec2 p = aspectUv(uv); float r = length(p);
  float a = P0 * (1.0 + uKick * P3 * 3.0);
  vec2 d = normalize(p + 1e-5) * sin(r * P1 - uTime * P2) * a;
  return texture(uIn, uv + d); })GLSL"));
    v.push_back (E ("fisheye", "Fisheye", "Distort", { { "Strength", -1, 1.5f, 0.5f }, { "Bass Pulse", 0, 1, 0.3f } },
        R"GLSL(
vec4 fx(vec2 uv) { vec2 p = uv * 2.0 - 1.0; float r = length(p); float k = P0 + uBass * P1;
  p *= mix(1.0, r * r, k * 0.5) / max(mix(1.0, 1.0, k), 1e-3);
  return texture(uIn, clamp(p * 0.5 + 0.5, 0.0, 1.0)); })GLSL"));
    v.push_back (E ("tunnel", "Tunnel Map", "Distort", { { "Speed", -4, 4, 0.5f }, { "Twist", -2, 2, 0.2f }, { "Depth", 0.1f, 4, 1 } },
        R"GLSL(
vec4 fx(vec2 uv) { vec2 p = aspectUv(uv); float r = length(p) + 1e-4; float a = atan(p.y, p.x);
  vec2 t = vec2(a / TAU + uTime * P1 * 0.1 + P1 / r * 0.0, P2 * 0.3 / r + uTime * P0);
  vec3 c = texture(uIn, fract(t)).rgb * smoothstep(0.0, 0.25, r);
  return vec4(c, 1.0); })GLSL"));
    v.push_back (E ("polar", "Polar Coordinates", "Distort", { { "Amount", 0, 1, 1 }, { "Rotation", 0, 1, 0 } },
        R"GLSL(
vec4 fx(vec2 uv) { vec2 p = aspectUv(uv); vec2 polar = vec2(fract(atan(p.y, p.x) / TAU + 0.5 + P1), clamp(length(p) * 1.4, 0.0, 1.0));
  return texture(uIn, mix(uv, polar, P0)); })GLSL"));
    v.push_back (E ("noise_warp", "Noise Warp", "Distort", { { "Amount", 0, 0.2f, 0.03f }, { "Scale", 0.5f, 20, 4 }, { "Speed", 0, 4, 0.5f }, { "Audio React", 0, 1, 0.5f } },
        R"GLSL(
vec4 fx(vec2 uv) { vec2 n = vec2(fbm(uv * P1 + uTime * P2), fbm(uv * P1 + 31.7 - uTime * P2)) - 0.5;
  return texture(uIn, uv + n * P0 * (1.0 + uLevel * P3 * 3.0)); })GLSL"));
    v.push_back (E ("wave_warp", "Wave Warp", "Distort", { { "Amplitude X", 0, 0.1f, 0.01f }, { "Amplitude Y", 0, 0.1f, 0.01f }, { "Frequency", 0.5f, 40, 8 }, { "Speed", 0, 10, 2 } },
        R"GLSL(
vec4 fx(vec2 uv) { return texture(uIn, uv + vec2(sin(uv.y * P2 + uTime * P3) * P0, cos(uv.x * P2 + uTime * P3) * P1) * (1.0 + uBass)); })GLSL"));
    v.push_back (E ("tile", "Tile", "Distort", { { "Tiles", 1, 16, 2 }, { "Mirror Tiles", 0, 1, 1 }, { "Scroll", -1, 1, 0 } },
        R"GLSL(
vec4 fx(vec2 uv) { vec2 p = uv * floor(P0) + vec2(uTime * P2, 0); vec2 f = fract(p);
  if (P1 > 0.5) { vec2 m = mod(floor(p), 2.0); f = mix(f, 1.0 - f, m); }
  return texture(uIn, f); })GLSL"));
    v.push_back (E ("transform", "Transform", "Distort", { { "Zoom", 0.1f, 4, 1 }, { "Rotate", -3.1416f, 3.1416f, 0 }, { "Offset X", -1, 1, 0 }, { "Offset Y", -1, 1, 0 }, { "Bass Pulse", 0, 1, 0 }, { "Spin Speed", -2, 2, 0 } },
        R"GLSL(
vec4 fx(vec2 uv) { vec2 p = aspectUv(uv) - vec2(P2, P3);
  p = rot2(P1 + uTime * P5) * p / (P0 * (1.0 + uBass * P4 * 0.5));
  vec2 q = p * vec2(uRes.y / uRes.x, 1.0) + 0.5;
  return (q.x < 0.0 || q.y < 0.0 || q.x > 1.0 || q.y > 1.0) ? vec4(0) : texture(uIn, q); })GLSL"));
    v.push_back (E ("audio_displace", "Spectrum Displace", "Distort", { { "Amount", 0, 0.3f, 0.05f }, { "Vertical", 0, 1, 0 } },
        R"GLSL(
vec4 fx(vec2 uv) { float s = P1 > 0.5 ? spec(uv.x) : spec(uv.y);
  return texture(uIn, uv + (P1 > 0.5 ? vec2(0, s) : vec2(s, 0)) * P0); })GLSL"));
    v.push_back (E ("zoom_blur", "Zoom Blur", "Distort", { { "Strength", 0, 0.5f, 0.1f }, { "Kick Boost", 0, 1, 0.6f } },
        R"GLSL(
vec4 fx(vec2 uv) { vec3 acc = vec3(0); float s = P0 * (1.0 + uKick * P1 * 2.0);
  for (int i = 0; i < 12; i++) { float k = 1.0 - s * float(i) / 12.0; acc += texture(uIn, (uv - 0.5) * k + 0.5).rgb; }
  return vec4(acc / 12.0, 1.0); })GLSL"));

    //==========================================================================
    // STYLIZE
    //==========================================================================
    v.push_back (E ("edges", "Edge Detect", "Stylize", { { "Strength", 0, 8, 2 }, { "Keep Color", 0, 1, 0.3f }, { "Neon Hue", 0, 1, 0.55f } },
        R"GLSL(
vec4 fx(vec2 uv) { vec2 px = 1.0 / uRes;
  float tl = luma(texture(uIn, uv + px * vec2(-1, 1)).rgb), t = luma(texture(uIn, uv + px * vec2(0, 1)).rgb), tr = luma(texture(uIn, uv + px * vec2(1, 1)).rgb);
  float l = luma(texture(uIn, uv + px * vec2(-1, 0)).rgb), r = luma(texture(uIn, uv + px * vec2(1, 0)).rgb);
  float bl = luma(texture(uIn, uv + px * vec2(-1, -1)).rgb), b = luma(texture(uIn, uv + px * vec2(0, -1)).rgb), br = luma(texture(uIn, uv + px * vec2(1, -1)).rgb);
  float gx = tr + 2.0 * r + br - tl - 2.0 * l - bl, gy = tl + 2.0 * t + tr - bl - 2.0 * b - br;
  float e = clamp(sqrt(gx * gx + gy * gy) * P0, 0.0, 1.0);
  vec3 col = mix(hsv2rgb(vec3(P2 + uTime * 0.05, 0.8, 1.0)), texture(uIn, uv).rgb * 2.0, P1);
  return vec4(col * e, 1.0); })GLSL"));
    v.push_back (E ("emboss", "Emboss", "Stylize", { { "Strength", 0, 8, 2 } },
        R"GLSL(
vec4 fx(vec2 uv) { vec2 px = 1.0 / uRes; vec3 a = texture(uIn, uv - px).rgb, b = texture(uIn, uv + px).rgb;
  return vec4(vec3(0.5) + (b - a) * P0, 1.0); })GLSL"));
    v.push_back (E ("halftone", "Halftone", "Stylize", { { "Dot Size (px)", 3, 40, 8 }, { "Angle", 0, 1.57f, 0.4f }, { "Colour", 0, 1, 1 } },
        R"GLSL(
vec4 fx(vec2 uv) { vec2 p = rot2(P1) * (uv * uRes) / P0; vec2 cell = floor(p) + 0.5;
  vec2 cuv = (rot2(-P1) * cell * P0) / uRes; vec3 c = texture(uIn, cuv).rgb;
  float d = length(fract(p) - 0.5); float r = sqrt(luma(c)) * 0.6 * (1.0 + uBass * 0.3);
  float m = smoothstep(r + 0.05, r - 0.05, d);
  return vec4(mix(vec3(m), c * m * 1.3, P2), 1.0); })GLSL"));
    v.push_back (E ("led", "LED Matrix", "Stylize", { { "Cell Size (px)", 3, 40, 10 }, { "Glow", 0, 2, 1 } },
        R"GLSL(
vec4 fx(vec2 uv) { vec2 cell = floor(uv * uRes / P0); vec2 f = fract(uv * uRes / P0) - 0.5;
  vec3 c = texture(uIn, (cell + 0.5) * P0 / uRes).rgb;
  float m = exp(-dot(f, f) * 12.0 / max(P1, 0.05));
  return vec4(c * m * 1.6, 1.0); })GLSL"));
    v.push_back (E ("crt", "CRT", "Stylize", { { "Curvature", 0, 0.5f, 0.15f }, { "Scanlines", 0, 1, 0.5f }, { "Vignette", 0, 1, 0.5f }, { "RGB Mask", 0, 1, 0.3f } },
        R"GLSL(
vec4 fx(vec2 uv) { vec2 p = uv * 2.0 - 1.0; p *= 1.0 + P0 * dot(p, p) * 0.3; vec2 q = p * 0.5 + 0.5;
  if (q.x < 0.0 || q.y < 0.0 || q.x > 1.0 || q.y > 1.0) return vec4(0, 0, 0, 1);
  vec3 c = texture(uIn, q).rgb;
  c *= 1.0 - P1 * 0.5 * (0.5 + 0.5 * sin(q.y * uRes.y * PI));
  float m = mod(floor(q.x * uRes.x), 3.0); vec3 mask = vec3(m == 0.0, m == 1.0, m == 2.0);
  c *= mix(vec3(1.0), mask * 1.8 + 0.2, P3);
  c *= 1.0 - P2 * dot(p, p) * 0.35;
  return vec4(c, 1.0); })GLSL"));
    v.push_back (E ("grain", "Film Grain", "Stylize", { { "Amount", 0, 0.5f, 0.08f }, { "Size", 1, 4, 1 } },
        R"GLSL(
vec4 fx(vec2 uv) { vec3 c = texture(uIn, uv).rgb; float n = hash21(floor(uv * uRes / P1) + fract(uTime * 7.13) * 100.0) - 0.5; return vec4(c + n * P0, 1.0); })GLSL"));
    v.push_back (E ("glow", "Bloom Glow", "Stylize", { { "Threshold", 0, 1, 0.6f }, { "Intensity", 0, 4, 1.2f }, { "Radius (px)", 1, 32, 8 }, { "Bass Boost", 0, 2, 0.5f } },
        R"GLSL(
vec4 fx(vec2 uv) { vec3 c = texture(uIn, uv).rgb; vec3 acc = vec3(0); float wsum = 0.0;
  for (int i = 0; i < 16; i++) { float a = float(i) * 2.39996; float r = sqrt(float(i) / 16.0) * P2;
    vec2 o = vec2(cos(a), sin(a)) * r / uRes; vec3 s = texture(uIn, uv + o).rgb;
    acc += max(s - P0, 0.0); wsum += 1.0; }
  return vec4(c + acc / wsum * P1 * (1.0 + uBass * P3) * 3.0, 1.0); })GLSL"));
    v.push_back (E ("blur", "Blur", "Stylize", { { "Radius (px)", 0, 40, 6 } },
        R"GLSL(
vec4 fx(vec2 uv) { vec3 acc = vec3(0);
  for (int i = 0; i < 24; i++) { float a = float(i) * 2.39996; float r = sqrt((float(i) + 0.5) / 24.0) * P0;
    acc += texture(uIn, uv + vec2(cos(a), sin(a)) * r / uRes).rgb; }
  return vec4(acc / 24.0, 1.0); })GLSL"));
    v.push_back (E ("sharpen", "Sharpen", "Stylize", { { "Amount", 0, 4, 1 } },
        R"GLSL(
vec4 fx(vec2 uv) { vec2 px = 1.0 / uRes; vec3 c = texture(uIn, uv).rgb;
  vec3 b = (texture(uIn, uv + vec2(px.x, 0)).rgb + texture(uIn, uv - vec2(px.x, 0)).rgb + texture(uIn, uv + vec2(0, px.y)).rgb + texture(uIn, uv - vec2(0, px.y)).rgb) * 0.25;
  return vec4(clamp(c + (c - b) * P0, 0.0, 1.0), 1.0); })GLSL"));
    v.push_back (E ("pixelate", "Pixelate", "Stylize", { { "Pixel Size", 1, 64, 8 }, { "Kick Boost", 0, 1, 0 } },
        R"GLSL(
vec4 fx(vec2 uv) { return texture(uIn, blockUv(uv, P0 * (1.0 + uKick * P1 * 3.0))); })GLSL"));
    v.push_back (E ("glyphs", "Glyph Matrix", "Stylize", { { "Cell Size (px)", 4, 32, 10 }, { "Colour", 0, 1, 1 }, { "Speed", 0, 8, 2 } },
        R"GLSL(
vec4 fx(vec2 uv) { vec2 cell = floor(uv * uRes / P0); vec2 f = fract(uv * uRes / P0);
  vec3 c = texture(uIn, (cell + 0.5) * P0 / uRes).rgb; float l = luma(c);
  vec2 g = floor(f * 4.0); float bit = step(0.5, hash21(g + cell * 7.0 + floor(uTime * P2 + hash21(cell) * 10.0)));
  float on = bit * step(0.15, l) * step(0.08, min(f.x, f.y)) * step(max(f.x, f.y), 0.92);
  return vec4(mix(vec3(0.2, 1.0, 0.4) * l, c, P1) * on * 1.5, 1.0); })GLSL"));
    v.push_back (E ("vignette", "Vignette", "Stylize", { { "Amount", 0, 2, 0.8f }, { "Softness", 0.05f, 1, 0.5f } },
        R"GLSL(
vec4 fx(vec2 uv) { vec3 c = texture(uIn, uv).rgb; float d = length(uv - 0.5) * 1.414; return vec4(c * (1.0 - smoothstep(1.0 - P1, 1.0, d) * P0), 1.0); })GLSL"));
    v.push_back (E ("rgb_split", "RGB Split", "Stylize", { { "Amount", 0, 0.05f, 0.006f }, { "Angle", 0, 6.283f, 0 }, { "Kick Boost", 0, 1, 0.5f } },
        R"GLSL(
vec4 fx(vec2 uv) { vec2 d = vec2(cos(P1), sin(P1)) * P0 * (1.0 + uKick * P2 * 4.0);
  return vec4(texture(uIn, uv + d).r, texture(uIn, uv).g, texture(uIn, uv - d).b, 1.0); })GLSL"));

    //==========================================================================
    // FEEDBACK / TIME
    //==========================================================================
    v.push_back (E ("feedback", "Feedback Zoom", "Feedback", { { "Amount", 0, 0.99f, 0.8f }, { "Zoom", 0.9f, 1.1f, 1.02f }, { "Rotate", -0.1f, 0.1f, 0.01f }, { "Hue Shift", 0, 0.1f, 0.0f }, { "Blend (0 max,1 add)", 0, 1, 0 } },
        R"GLSL(
vec4 fx(vec2 uv) { vec4 cur = texture(uIn, uv); vec2 p = aspectUv(uv);
  p = rot2(P2) * p / P1; vec3 prev = texture(uPrev, p * vec2(uRes.y / uRes.x, 1.0) + 0.5).rgb;
  vec3 h = rgb2hsv(prev); h.x = fract(h.x + P3); prev = hsv2rgb(h) * P0;
  return vec4(P4 > 0.5 ? clamp(cur.rgb + prev, 0.0, 1.0) : max(cur.rgb, prev), 1.0); })GLSL", true));
    v.push_back (E ("strobe", "Strobe", "Feedback", { { "Division (per beat)", 0.25f, 8, 1 }, { "Duty", 0.05f, 0.95f, 0.3f }, { "Mode (0 black,1 white,2 invert)", 0, 2, 0 } },
        R"GLSL(
vec4 fx(vec2 uv) { vec4 c = texture(uIn, uv); bool on = fract(uBeats * P0) < P1; int m = int(P2 + 0.5);
  if (!on) return c; if (m == 0) return vec4(0, 0, 0, 1); if (m == 1) return vec4(1); return vec4(1.0 - c.rgb, 1); })GLSL"));
    v.push_back (E ("beat_flash", "Beat Flash", "Feedback", { { "Intensity", 0, 2, 0.6f }, { "Source (0 beat,1 kick)", 0, 1, 1 }, { "Hue", 0, 1, 0.0f }, { "Saturation", 0, 1, 0 } },
        R"GLSL(
vec4 fx(vec2 uv) { vec4 c = texture(uIn, uv); float e = P1 > 0.5 ? uKick : uPulse;
  return vec4(c.rgb + hsv2rgb(vec3(P2, P3, 1.0)) * e * e * P0, 1.0); })GLSL"));
    v.push_back (E ("trails", "Trails", "Feedback", { { "Persistence", 0, 0.99f, 0.9f } },
        R"GLSL(
vec4 fx(vec2 uv) { vec4 c = texture(uIn, uv); return vec4(mix(c.rgb, texture(uPrev, uv).rgb, P0 * (1.0 - luma(c.rgb) * 0.5)), 1.0); })GLSL", true));
    v.push_back (E ("time_slice", "Time Displace", "Feedback", { { "Amount", 0, 1, 0.6f }, { "Bands", 1, 64, 12 } },
        R"GLSL(
vec4 fx(vec2 uv) { float k = fract(floor(uv.y * P1) / P1 + uTime * 0.1); return mix(texture(uIn, uv), texture(uPrev, uv), k * P0); })GLSL", true));

    //==========================================================================
    // KEYING / MASK
    //==========================================================================
    v.push_back (E ("luma_key", "Luma Key", "Keying", { { "Threshold", 0, 1, 0.1f }, { "Softness", 0, 0.5f, 0.05f }, { "Invert", 0, 1, 0 } },
        R"GLSL(
vec4 fx(vec2 uv) { vec4 c = texture(uIn, uv); float a = smoothstep(P0 - P1, P0 + P1, luma(c.rgb)); if (P2 > 0.5) a = 1.0 - a; return vec4(c.rgb * a, a); })GLSL"));
    v.push_back (E ("chroma_key", "Chroma Key", "Keying", { { "Key Hue", 0, 1, 0.33f }, { "Tolerance", 0, 0.5f, 0.1f }, { "Softness", 0, 0.3f, 0.05f } },
        R"GLSL(
vec4 fx(vec2 uv) { vec4 c = texture(uIn, uv); vec3 h = rgb2hsv(c.rgb); float d = abs(fract(h.x - P0 + 0.5) - 0.5);
  float a = smoothstep(P1, P1 + P2, d) + (1.0 - smoothstep(0.15, 0.3, h.y)); a = clamp(a, 0.0, 1.0); return vec4(c.rgb * a, a); })GLSL"));
    v.push_back (E ("audio_mask", "Spectrum Mask", "Keying", { { "Height", 0, 2, 1 }, { "Mirror", 0, 1, 1 } },
        R"GLSL(
vec4 fx(vec2 uv) { vec4 c = texture(uIn, uv); float x = P1 > 0.5 ? abs(uv.x - 0.5) * 2.0 : uv.x;
  float a = step(abs(uv.y - 0.5) * 2.0, spec(x) * P0); return vec4(c.rgb * a, a); })GLSL"));
    v.push_back (E ("circle_mask", "Circle Mask", "Keying", { { "Radius", 0, 1, 0.35f }, { "Softness", 0, 0.3f, 0.01f }, { "Bass Pulse", 0, 1, 0.3f } },
        R"GLSL(
vec4 fx(vec2 uv) { vec4 c = texture(uIn, uv); float r = P0 * (1.0 + uBass * P2 * 0.5);
  float a = 1.0 - smoothstep(r - P1, r + P1, length(aspectUv(uv))); return vec4(c.rgb * a, a); })GLSL"));

    return v;
}

const std::vector<ShaderDef>& EffectLibrary::all()
{
    static const std::vector<ShaderDef> effects = buildEffects();
    return effects;
}

const ShaderDef* EffectLibrary::find (const juce::String& id)
{
    for (auto& e : all())
        if (e.id == id)
            return &e;
    return nullptr;
}

juce::StringArray EffectLibrary::categories()
{
    juce::StringArray c;
    for (auto& e : all())
        c.addIfNotAlreadyThere (e.category);
    return c;
}
} // namespace moshdeck
