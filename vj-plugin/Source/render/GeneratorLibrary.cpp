#include "EffectLibrary.h"

namespace moshdeck
{
static ShaderDef G (const char* id, const char* name, const char* cat, std::initializer_list<P> params, const char* glsl, bool history = false)
{
    ShaderDef d;
    d.id = id; d.name = name; d.category = cat;
    d.params = makeParams (params);
    d.glsl = glsl;
    d.usesHistory = history;
    return d;
}

// Every generator receives the layer's base input in uIn ("Base" param = how much of it is used).
static std::vector<ShaderDef> buildGenerators()
{
    std::vector<ShaderDef> v;

    v.push_back (G ("plasma", "Plasma", "Classic",
        { { "Speed", 0, 4, 1 }, { "Scale", 0.5f, 10, 3 }, { "Hue Speed", 0, 1, 0.1f }, { "Base Mix", 0, 1, 0.0f }, { "Base Warp", 0, 0.2f, 0.03f }, { "Bass React", 0, 2, 1 } },
        R"GLSL(
vec4 gen(vec2 uv) {
    vec2 p = aspectUv(uv) * P1; float t = uTime * P0;
    float v = sin(p.x + t) + sin(p.y * 1.3 - t * 0.7) + sin((p.x + p.y) * 0.7 + t * 1.3) + sin(length(p) * 1.5 - t * (1.0 + uBass * P5));
    vec3 col = rainbow(v * 0.125 + uTime * P2);
    vec3 base = texture(uIn, uv + vec2(sin(v), cos(v)) * P4).rgb;
    return vec4(mix(col, col * base * 2.0, P3), 1.0);
})GLSL"));

    v.push_back (G ("tunnel", "Hyper Tunnel", "Classic",
        { { "Speed", -4, 4, 1 }, { "Twist", -3, 3, 0.5f }, { "Rings", 1, 30, 8 }, { "Base Mix", 0, 1, 0.5f }, { "Hue", 0, 1, 0.6f }, { "Kick Flash", 0, 2, 1 } },
        R"GLSL(
vec4 gen(vec2 uv) {
    vec2 p = aspectUv(uv); float r = length(p) + 1e-4; float a = atan(p.y, p.x);
    float z = 1.0 / r + uTime * P0;
    vec2 t = vec2(a / TAU + P1 / r * 0.1 + uTime * 0.05, z * 0.2);
    float ring = 0.5 + 0.5 * sin(z * P2);
    vec3 col = hsv2rgb(vec3(fract(P4 + z * 0.02), 0.7, ring)) * (1.0 + uKick * P5);
    vec3 tex = texture(uIn, fract(t)).rgb;
    col = mix(col, tex * (0.5 + ring), P3);
    return vec4(col * smoothstep(0.0, 0.3, r), 1.0);
})GLSL"));

    v.push_back (G ("starfield", "Star Warp", "Space",
        { { "Speed", 0, 4, 1 }, { "Density", 1, 8, 4 }, { "Trail", 0, 1, 0.5f }, { "Base Mix", 0, 1, 0.3f }, { "Bass Boost", 0, 4, 2 } },
        R"GLSL(
vec4 gen(vec2 uv) {
    vec2 p = aspectUv(uv); vec3 col = vec3(0);
    float spd = uTime * P0 * (1.0 + uBass * P4 * 0.25);
    for (int L = 0; L < 6; L++) {
        float fl = float(L);
        if (fl >= P1 * 1.5) break;
        float depth = fract(fl / 6.0 + spd * 0.15);
        float scale = mix(20.0, 0.5, depth);
        vec2 q = p * scale + fl * 17.3;
        vec2 id = floor(q), f = fract(q) - 0.5;
        float h = hash21(id);
        vec2 o = (hash22(id) - 0.5) * 0.7;
        float d = length(f - o);
        float s = smoothstep(0.08 * (1.0 + P2 * depth), 0.0, d) * step(0.6, h) * depth * (1.0 - depth) * 4.0;
        col += s * hsv2rgb(vec3(h, 0.3, 1.0));
    }
    vec3 base = texture(uIn, uv).rgb * (0.3 + uLevel);
    return vec4(col + base * P3, 1.0);
})GLSL"));

    v.push_back (G ("voronoi", "Voronoi Cells", "Organic",
        { { "Scale", 1, 20, 6 }, { "Speed", 0, 4, 0.6f }, { "Edge Glow", 0, 4, 1.5f }, { "Base Mix", 0, 1, 0.7f }, { "Hue", 0, 1, 0.5f }, { "Bass Pulse", 0, 1, 0.5f } },
        R"GLSL(
vec4 gen(vec2 uv) {
    vec2 p = aspectUv(uv) * P0;
    vec2 id = floor(p), f = fract(p);
    float d1 = 8.0, d2 = 8.0; vec2 bestId = vec2(0);
    for (int y = -1; y <= 1; y++) for (int x = -1; x <= 1; x++) {
        vec2 g = vec2(x, y); vec2 o = hash22(id + g);
        o = 0.5 + 0.5 * sin(uTime * P1 + TAU * o);
        float d = length(g + o - f);
        if (d < d1) { d2 = d1; d1 = d; bestId = id + g; } else if (d < d2) d2 = d;
    }
    float edge = 1.0 - smoothstep(0.0, 0.08 * (1.0 + uBass * P5), d2 - d1);
    vec2 cellUv = (bestId / P0) * vec2(uRes.y / uRes.x, 1.0) + 0.5;
    vec3 base = texture(uIn, fract(cellUv)).rgb;
    vec3 col = mix(hsv2rgb(vec3(fract(P4 + hash21(bestId) * 0.3), 0.6, 0.5 + 0.5 * spec(hash21(bestId)))), base, P3);
    return vec4(col + edge * P2 * rainbow(uTime * 0.1 + d1), 1.0);
})GLSL"));

    v.push_back (G ("mandel", "Fractal Zoom", "Fractal",
        { { "Zoom Speed", 0, 2, 0.3f }, { "Iterations", 16, 200, 80 }, { "Colour Speed", 0, 2, 0.3f }, { "Base Mix", 0, 1, 0.2f }, { "Target", 0, 3, 0 } },
        R"GLSL(
vec4 gen(vec2 uv) {
    vec2 targets[4] = vec2[4](vec2(-0.743643887, 0.131825904), vec2(-0.1011, 0.9563), vec2(-1.25066, 0.02012), vec2(0.360240443, -0.641313061));
    vec2 c0 = targets[int(clamp(P4, 0.0, 3.0))];
    float zoom = exp(-mod(uTime * P0, 14.0));
    vec2 c = c0 + aspectUv(uv) * 3.0 * zoom;
    vec2 z = vec2(0); float n = 0.0;
    for (int i = 0; i < 200; i++) { if (float(i) > P1) break; z = vec2(z.x * z.x - z.y * z.y, 2.0 * z.x * z.y) + c; if (dot(z, z) > 64.0) break; n += 1.0; }
    float sm = n - log2(log2(dot(z, z) + 1.0)) + 4.0;
    vec3 col = n >= P1 ? vec3(0) : rainbow(sm * 0.03 + uTime * P2) * (0.6 + uBass * 0.6);
    vec3 base = texture(uIn, fract(z * 0.05)).rgb;
    return vec4(mix(col, col * base * 2.0, P3), 1.0);
})GLSL"));

    v.push_back (G ("julia", "Julia Morph", "Fractal",
        { { "Morph Speed", 0, 2, 0.2f }, { "Zoom", 0.3f, 3, 1.2f }, { "Iterations", 16, 160, 64 }, { "Base Mix", 0, 1, 0.3f }, { "Audio Morph", 0, 1, 0.3f } },
        R"GLSL(
vec4 gen(vec2 uv) {
    float t = uTime * P0;
    vec2 c = vec2(0.7885 * cos(t), 0.7885 * sin(t)) + vec2(uBass, uMid) * P4 * 0.1;
    vec2 z = aspectUv(uv) * 3.0 / P1; float n = 0.0;
    for (int i = 0; i < 160; i++) { if (float(i) > P2) break; z = vec2(z.x * z.x - z.y * z.y, 2.0 * z.x * z.y) + c; if (dot(z, z) > 16.0) break; n += 1.0; }
    vec3 col = rainbow(n / P2 * 2.0 + uTime * 0.1) * smoothstep(0.0, 4.0, n);
    vec3 base = texture(uIn, fract(z * 0.1 + 0.5)).rgb;
    return vec4(mix(col, base * col * 2.5, P3), 1.0);
})GLSL"));

    v.push_back (G ("metaballs", "Metaballs", "Organic",
        { { "Balls", 2, 12, 6 }, { "Size", 0.02f, 0.3f, 0.12f }, { "Speed", 0, 3, 0.7f }, { "Base Mix", 0, 1, 0.5f }, { "Hue", 0, 1, 0.8f }, { "Bass Grow", 0, 2, 1 } },
        R"GLSL(
vec4 gen(vec2 uv) {
    vec2 p = aspectUv(uv); float f = 0.0;
    for (int i = 0; i < 12; i++) { if (float(i) >= P0) break; float fi = float(i);
        vec2 c = vec2(sin(uTime * P2 * (0.5 + hash11(fi)) + fi * 2.1), cos(uTime * P2 * (0.4 + hash11(fi + 3.0)) + fi * 1.3)) * 0.45;
        float r = P1 * (1.0 + spec(fi / 12.0) * P5);
        f += r * r / max(dot(p - c, p - c), 1e-4); }
    float m = smoothstep(0.9, 1.1, f);
    vec3 col = hsv2rgb(vec3(fract(P4 + f * 0.05 + uTime * 0.05), 0.8, 1.0)) * m;
    vec3 base = texture(uIn, uv + p * (f - 1.0) * 0.01).rgb;
    return vec4(mix(col, base * (0.4 + m), P3), 1.0);
})GLSL"));

    v.push_back (G ("sinewaves", "Oscilloscope Waves", "Audio",
        { { "Lines", 1, 24, 8 }, { "Thickness", 0.001f, 0.05f, 0.006f }, { "Amplitude", 0, 1, 0.3f }, { "Base Mix", 0, 1, 0.3f }, { "Hue Spread", 0, 1, 0.5f } },
        R"GLSL(
vec4 gen(vec2 uv) {
    vec3 col = vec3(0);
    for (int i = 0; i < 24; i++) { if (float(i) >= P0) break; float fi = float(i) / max(P0, 1.0);
        float y = 0.5 + (fi - 0.5) * 0.6 + wave(fract(uv.x + fi * 0.3)) * P2 * (0.5 + fi);
        float d = abs(uv.y - y);
        col += hsv2rgb(vec3(fi * P4 + uTime * 0.05, 0.8, 1.0)) * smoothstep(P1, 0.0, d) * 1.5; }
    return vec4(col + texture(uIn, uv).rgb * P3, 1.0);
})GLSL"));

    v.push_back (G ("grid", "Retro Grid", "Space",
        { { "Speed", 0, 4, 1 }, { "Horizon", 0.2f, 0.8f, 0.45f }, { "Grid Density", 2, 40, 12 }, { "Base Mix (sky)", 0, 1, 1 }, { "Hue", 0, 1, 0.85f }, { "Kick Glow", 0, 2, 1 } },
        R"GLSL(
vec4 gen(vec2 uv) {
    vec3 col;
    if (uv.y < P1) {
        float y = (P1 - uv.y) / P1; float z = 1.0 / max(y, 1e-3);
        vec2 g = vec2((uv.x - 0.5) * z, z + uTime * P0 * 2.0) * P2 * 0.1;
        vec2 l = abs(fract(g) - 0.5);
        float line = smoothstep(0.05 * z * 0.3, 0.0, min(l.x, l.y));
        col = hsv2rgb(vec3(P4, 0.8, 1.0)) * line * (y * 1.5) * (1.0 + uKick * P5);
    } else {
        vec2 suv = vec2(uv.x, (uv.y - P1) / (1.0 - P1));
        vec3 sky = mix(hsv2rgb(vec3(P4 - 0.15, 0.9, 0.6)), vec3(0.02, 0.0, 0.08), suv.y);
        float sun = smoothstep(0.26, 0.25, length((suv - vec2(0.5, 0.3)) * vec2(uRes.x / uRes.y, 1.0))) * step(0.5, fract(suv.y * 20.0 - uTime * 0.5) + suv.y);
        sky += vec3(1.0, 0.5, 0.2) * sun;
        col = mix(sky, texture(uIn, suv).rgb, P3 * 0.8);
    }
    return vec4(col, 1.0);
})GLSL"));

    v.push_back (G ("clouds", "Nebula Clouds", "Space",
        { { "Speed", 0, 2, 0.3f }, { "Scale", 0.5f, 8, 2.5f }, { "Hue", 0, 1, 0.65f }, { "Base Mix", 0, 1, 0.4f }, { "Contrast", 0.5f, 4, 1.5f } },
        R"GLSL(
vec4 gen(vec2 uv) {
    vec2 p = aspectUv(uv) * P1;
    float n = fbm(p + fbm(p + uTime * P0) * (1.0 + uBass));
    n = pow(n, P4);
    vec3 col = mix(hsv2rgb(vec3(P2, 0.8, 0.1)), hsv2rgb(vec3(fract(P2 + 0.3 + uMid * 0.2), 0.6, 1.0)), n);
    return vec4(mix(col, texture(uIn, uv + (n - 0.5) * 0.05).rgb * (0.4 + n), P3), 1.0);
})GLSL"));

    v.push_back (G ("hexgrid", "Hex Pulse", "Geometric",
        { { "Scale", 2, 30, 10 }, { "Pulse Speed", 0, 4, 1 }, { "Line Width", 0.01f, 0.3f, 0.06f }, { "Base Mix", 0, 1, 0.6f }, { "Hue", 0, 1, 0.5f } },
        R"GLSL(
vec2 hexCoord(vec2 p, out vec2 id) { vec2 r = vec2(1.0, 1.7320508); vec2 h = r * 0.5;
  vec2 a = mod(p, r) - h, b = mod(p - h, r) - h; vec2 g = dot(a, a) < dot(b, b) ? a : b; id = p - g; return g; }
vec4 gen(vec2 uv) {
    vec2 id; vec2 g = hexCoord(aspectUv(uv) * P0, id);
    float d = max(abs(g.x) * 0.866 + abs(g.y) * 0.5, abs(g.y));
    float e = smoothstep(0.5 - P2, 0.5, d * 1.155);
    float wavev = 0.5 + 0.5 * sin(length(id) * 0.6 - uTime * P1 * 3.0);
    float s = spec(fract(length(id) * 0.05));
    vec3 cell = hsv2rgb(vec3(fract(P4 + length(id) * 0.02), 0.7, wavev * (0.3 + s)));
    vec3 base = texture(uIn, uv).rgb;
    return vec4(mix(cell, base * (wavev + s), P3) + e * rainbow(uTime * 0.1 + length(id) * 0.05) * 0.8, 1.0);
})GLSL"));

    v.push_back (G ("spiral", "Hypno Spiral", "Geometric",
        { { "Arms", 1, 16, 4 }, { "Speed", -6, 6, 2 }, { "Tightness", 1, 40, 12 }, { "Base Mix", 0, 1, 0.3f }, { "Hue", 0, 1, 0.0f } },
        R"GLSL(
vec4 gen(vec2 uv) {
    vec2 p = aspectUv(uv); float r = length(p), a = atan(p.y, p.x);
    float s = sin(a * floor(P0) + log(r + 1e-3) * P2 - uTime * P1 * (1.0 + uBass));
    float m = smoothstep(-0.1, 0.1, s);
    vec3 col = mix(hsv2rgb(vec3(P4, 0.8, 0.1)), rainbow(r + uTime * 0.2 + P4), m);
    return vec4(mix(col, texture(uIn, uv).rgb * m * 1.5, P3), 1.0);
})GLSL"));

    v.push_back (G ("lasers", "Laser Show", "Audio",
        { { "Beams", 2, 32, 12 }, { "Sweep Speed", 0, 4, 1 }, { "Width", 0.001f, 0.03f, 0.004f }, { "Base Mix", 0, 1, 0.2f }, { "Hue", 0, 1, 0.3f }, { "Kick Strobe", 0, 1, 0.5f } },
        R"GLSL(
vec4 gen(vec2 uv) {
    vec2 o = vec2(0.5, -0.05); vec2 d = uv - o; d.x *= uRes.x / uRes.y;
    float a = atan(d.x, d.y); vec3 col = vec3(0);
    for (int i = 0; i < 32; i++) { if (float(i) >= P0) break; float fi = float(i) / P0;
        float ba = (fi - 0.5) * 1.6 + sin(uTime * P1 + fi * 6.0) * 0.4;
        float w = abs(a - ba);
        col += hsv2rgb(vec3(fract(P4 + fi * 0.3), 0.8, 1.0)) * smoothstep(P2 * 8.0, 0.0, w) * (0.3 + spec(fi) * 1.5); }
    col *= 1.0 - P5 + P5 * (0.3 + uKick);
    return vec4(col + texture(uIn, uv).rgb * P3, 1.0);
})GLSL"));

    v.push_back (G ("liquid", "Liquid Chrome", "Organic",
        { { "Speed", 0, 3, 0.5f }, { "Scale", 0.5f, 8, 2 }, { "Shine", 0, 4, 1.5f }, { "Base Mix (reflection)", 0, 1, 0.8f }, { "Bass Ripple", 0, 2, 1 } },
        R"GLSL(
vec4 gen(vec2 uv) {
    vec2 p = aspectUv(uv) * P1; float t = uTime * P0;
    float h = fbm(p + vec2(t, -t * 0.7)) + sin(length(p) * 6.0 - uTime * 4.0) * 0.05 * uBass * P4;
    vec2 grad = vec2(fbm(p + vec2(0.01, 0) + vec2(t, -t * 0.7)) - h, fbm(p + vec2(0, 0.01) + vec2(t, -t * 0.7)) - h) * 100.0;
    vec3 n = normalize(vec3(-grad, 1.0));
    vec3 env = texture(uIn, fract(uv + n.xy * 0.15)).rgb;
    vec3 chrome = vec3(0.5 + 0.5 * n.y) * vec3(0.8, 0.85, 1.0);
    float spec2 = pow(max(dot(n, normalize(vec3(0.3, 0.5, 1.0))), 0.0), 32.0) * P2;
    return vec4(mix(chrome, env, P3) + spec2, 1.0);
})GLSL"));

    v.push_back (G ("radial_bars", "Radial Spectrum Burst", "Audio",
        { { "Inner Radius", 0, 0.5f, 0.15f }, { "Length", 0, 1, 0.4f }, { "Bars", 16, 128, 64 }, { "Base Mix (center)", 0, 1, 1 }, { "Hue", 0, 1, 0.55f }, { "Rotate Speed", -2, 2, 0.1f } },
        R"GLSL(
vec4 gen(vec2 uv) {
    vec2 p = aspectUv(uv); float r = length(p); float a = atan(p.y, p.x) + uTime * P5;
    float x = fract(a / TAU + 0.5); float bar = floor(x * P2) / P2;
    float s = spec(abs(bar - 0.5) * 2.0);
    float inner = P0 * (1.0 + uBass * 0.3);
    float on = step(inner, r) * step(r, inner + s * P1 + 0.005) * step(0.2, fract(x * P2));
    vec3 col = hsv2rgb(vec3(fract(P4 + bar * 0.5), 0.8, 1.0)) * on;
    vec2 cuv = p / max(inner, 1e-3) * 0.5 + 0.5;
    vec3 center = r < inner ? texture(uIn, cuv).rgb * P3 : vec3(0);
    return vec4(col + center, 1.0);
})GLSL"));

    v.push_back (G ("rings", "Bass Rings", "Audio",
        { { "Ring Count", 1, 30, 10 }, { "Speed", 0, 4, 1 }, { "Thickness", 0.005f, 0.2f, 0.03f }, { "Base Mix", 0, 1, 0.3f }, { "Hue", 0, 1, 0.1f } },
        R"GLSL(
vec4 gen(vec2 uv) {
    vec2 p = aspectUv(uv); float r = length(p);
    float f = fract(r * P0 - uTime * P1 * 0.5);
    float s = spec(fract(r));
    float ring = smoothstep(P2 * (1.0 + uBass * 2.0), 0.0, abs(f - 0.5) / P0 * 2.0);
    vec3 col = hsv2rgb(vec3(fract(P4 + r * 0.3 + uTime * 0.03), 0.8, 1.0)) * ring * (0.4 + s * 2.0);
    return vec4(col + texture(uIn, uv).rgb * P3 * (0.5 + ring), 1.0);
})GLSL"));

    v.push_back (G ("truchet", "Truchet Flow", "Geometric",
        { { "Scale", 2, 40, 12 }, { "Line Width", 0.02f, 0.3f, 0.08f }, { "Flip Rate", 0, 8, 1 }, { "Base Mix", 0, 1, 0.5f }, { "Hue", 0, 1, 0.4f } },
        R"GLSL(
vec4 gen(vec2 uv) {
    vec2 p = aspectUv(uv) * P0; vec2 id = floor(p); vec2 f = fract(p) - 0.5;
    if (hash21(id + floor(uBeats * P2 * 0.25)) > 0.5) f.x = -f.x;
    float d = abs(abs(f.x + f.y) - 0.5) * 0.7071;
    d = min(d, abs(length(f - 0.5) - 0.5));
    d = min(d, abs(length(f + 0.5) - 0.5));
    float l = smoothstep(P1, P1 * 0.5, d);
    vec3 col = rainbow(P4 + hash21(id) * 0.2 + uTime * 0.05) * l * (0.5 + uLevel);
    return vec4(mix(col, texture(uIn, uv).rgb * (0.3 + l), P3), 1.0);
})GLSL"));

    v.push_back (G ("flowfield", "Flow Field Paint", "Organic",
        { { "Speed", 0, 4, 1 }, { "Scale", 0.5f, 10, 3 }, { "Persistence", 0.8f, 0.995f, 0.96f }, { "Base Mix (paint source)", 0, 1, 0.8f }, { "Bass Turbulence", 0, 2, 1 } },
        R"GLSL(
vec4 gen(vec2 uv) {
    float a = fbm(uv * P1 + uTime * 0.1) * TAU * 2.0 * (1.0 + uBass * P4 * 0.5);
    vec2 dir = vec2(cos(a), sin(a)) * P0 * 0.002;
    vec3 prev = texture(uPrev, uv - dir).rgb;
    vec3 src = mix(rainbow(fbm(uv * 3.0 - uTime * 0.2) + uTime * 0.1), texture(uIn, uv).rgb, P3);
    float inject = 0.03 + uKick * 0.2;
    return vec4(mix(prev * P2, src, inject), 1.0);
})GLSL", true));

    v.push_back (G ("cubes", "Raymarch Cubes", "3D",
        { { "Speed", 0, 4, 1 }, { "Spacing", 1.5f, 6, 3 }, { "Twist", 0, 2, 0.3f }, { "Base Mix (texture)", 0, 1, 0.7f }, { "Bass Size", 0, 1, 0.3f } },
        R"GLSL(
float sdBox(vec3 p, vec3 b) { vec3 q = abs(p) - b; return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0); }
float mapC(vec3 p) { p.xy = rot2(p.z * P2 * 0.1) * p.xy; vec3 q = mod(p, P1) - P1 * 0.5; return sdBox(q, vec3(0.4 + uBass * P4 * 0.3)); }
vec4 gen(vec2 uv) {
    vec2 p = aspectUv(uv);
    vec3 ro = vec3(0.0, 0.0, uTime * P0 * 2.0), rd = normalize(vec3(p, 1.2));
    float t = 0.0; int i;
    for (i = 0; i < 72; i++) { float d = mapC(ro + rd * t); if (d < 0.002 || t > 40.0) break; t += d; }
    vec3 pos = ro + rd * t;
    vec2 e = vec2(0.002, 0);
    vec3 n = normalize(vec3(mapC(pos + e.xyy) - mapC(pos - e.xyy), mapC(pos + e.yxy) - mapC(pos - e.yxy), mapC(pos + e.yyx) - mapC(pos - e.yyx)));
    vec3 tex = texture(uIn, fract(pos.xy * 0.5 + pos.z * 0.1)).rgb;
    vec3 col = mix(rainbow(pos.z * 0.05 + uTime * 0.05), tex, P3) * (0.3 + 0.7 * max(dot(n, normalize(vec3(0.5, 0.8, -0.6))), 0.0));
    col *= exp(-t * 0.06);
    return vec4(col, 1.0);
})GLSL"));

    v.push_back (G ("spheres", "Raymarch Orbs", "3D",
        { { "Speed", 0, 4, 0.8f }, { "Orb Size", 0.1f, 1, 0.5f }, { "Glow", 0, 4, 1 }, { "Base Mix (env map)", 0, 1, 0.8f }, { "Hue", 0, 1, 0.6f } },
        R"GLSL(
float mapS(vec3 p) { vec3 q = p; q.xz = mod(q.xz + 2.0, 4.0) - 2.0; q.y += sin(p.x * 0.5 + uTime * P0) * 0.6;
  return length(q) - P1 * (1.0 + uBass * 0.4); }
vec4 gen(vec2 uv) {
    vec2 p = aspectUv(uv);
    vec3 ro = vec3(uTime * P0, 1.0, uTime * P0 * 1.5), rd = normalize(vec3(p.x, p.y - 0.2, 1.0));
    float t = 0.0, glow = 0.0;
    for (int i = 0; i < 80; i++) { float d = mapS(ro + rd * t); glow += 0.02 / (0.05 + abs(d)); if (d < 0.002 || t > 50.0) break; t += d; }
    vec3 pos = ro + rd * t; vec2 e = vec2(0.002, 0);
    vec3 n = normalize(vec3(mapS(pos + e.xyy) - mapS(pos - e.xyy), mapS(pos + e.yxy) - mapS(pos - e.yxy), mapS(pos + e.yyx) - mapS(pos - e.yyx)));
    vec3 r = reflect(rd, n);
    vec3 env = texture(uIn, fract(r.xy * 0.5 + 0.5)).rgb;
    vec3 col = mix(hsv2rgb(vec3(P4, 0.6, 0.8)) * (0.5 + 0.5 * n.y), env, P3) * exp(-t * 0.04);
    col += hsv2rgb(vec3(fract(P4 + 0.2), 0.7, 1.0)) * glow * 0.01 * P2 * (0.5 + uLevel);
    return vec4(col, 1.0);
})GLSL"));

    v.push_back (G ("waveform_tunnel", "Waveform Tunnel", "Audio",
        { { "Speed", 0, 4, 1 }, { "Rings", 4, 60, 24 }, { "Amplitude", 0, 0.5f, 0.12f }, { "Base Mix", 0, 1, 0.3f }, { "Hue", 0, 1, 0.75f } },
        R"GLSL(
vec4 gen(vec2 uv) {
    vec2 p = aspectUv(uv); float r = length(p); float a = atan(p.y, p.x);
    float z = 0.3 / max(r, 1e-3) + uTime * P0;
    float ringIdx = floor(z * P1 * 0.1);
    float w = wave(fract(a / TAU + 0.5)) * P2;
    float f = abs(fract(z * P1 * 0.1) - 0.5 + w);
    vec3 col = hsv2rgb(vec3(fract(P4 + ringIdx * 0.05), 0.8, 1.0)) * smoothstep(0.08, 0.0, f) * smoothstep(0.02, 0.2, r);
    return vec4(col + texture(uIn, uv).rgb * P3, 1.0);
})GLSL"));

    v.push_back (G ("blobs", "Lava Lamp", "Organic",
        { { "Speed", 0, 2, 0.3f }, { "Scale", 1, 8, 3 }, { "Hue", 0, 1, 0.02f }, { "Base Mix", 0, 1, 0.5f }, { "Smoothness", 0.01f, 0.5f, 0.1f } },
        R"GLSL(
vec4 gen(vec2 uv) {
    vec2 p = aspectUv(uv) * P1;
    float n = fbm(p * 0.7 + vec2(0.0, -uTime * P0)) + fbm(p * 1.3 + vec2(uTime * P0 * 0.5, 0.0)) * 0.5;
    float m = smoothstep(0.75 - P4, 0.75 + P4, n + uBass * 0.15);
    vec3 col = mix(hsv2rgb(vec3(fract(P2 + 0.6), 0.9, 0.15)), hsv2rgb(vec3(P2, 0.9, 1.0)), m);
    return vec4(mix(col, texture(uIn, uv).rgb * (0.3 + m * 1.2), P3), 1.0);
})GLSL"));

    v.push_back (G ("checker", "Warp Checker", "Geometric",
        { { "Squares", 2, 40, 10 }, { "Warp", 0, 2, 0.5f }, { "Speed", 0, 4, 1 }, { "Base Mix", 0, 1, 0.5f }, { "Beat Flip", 0, 1, 1 } },
        R"GLSL(
vec4 gen(vec2 uv) {
    vec2 p = aspectUv(uv); float r = length(p);
    p *= 1.0 + sin(r * 4.0 - uTime * P2) * P1 * 0.3 * (0.5 + uBass);
    vec2 c = floor(p * P0 + uTime * P2 * 0.3);
    float k = mod(c.x + c.y + (P4 > 0.5 ? floor(uBeats) : 0.0), 2.0);
    vec3 base = texture(uIn, uv).rgb;
    vec3 col = mix(vec3(k), mix(base, 1.0 - base, k), P3);
    return vec4(col, 1.0);
})GLSL"));

    v.push_back (G ("solid", "Solid / Gradient", "Utility",
        { { "Hue", 0, 1, 0.6f }, { "Saturation", 0, 1, 0.8f }, { "Brightness", 0, 1, 0.5f }, { "Gradient", 0, 1, 0.5f }, { "Hue Cycle", 0, 1, 0 } },
        R"GLSL(
vec4 gen(vec2 uv) {
    float h = fract(P0 + uTime * P4 * 0.2);
    vec3 a = hsv2rgb(vec3(h, P1, P2)), b = hsv2rgb(vec3(fract(h + 0.3), P1, P2 * 0.4));
    return vec4(mix(a, b, uv.y * P3), 1.0);
})GLSL"));

    return v;
}

const std::vector<ShaderDef>& GeneratorLibrary::all()
{
    static const std::vector<ShaderDef> gens = buildGenerators();
    return gens;
}

const ShaderDef* GeneratorLibrary::get (int index)
{
    auto& a = all();
    return index >= 0 && index < (int) a.size() ? &a[(size_t) index] : nullptr;
}
} // namespace moshdeck
