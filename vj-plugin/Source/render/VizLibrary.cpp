#include "VizLibrary.h"
#include "model/Scene.h"

namespace moshdeck
{
// Shared params: P0 Size, P1 X, P2 Y, P3 Bass React, P4 Hue, P5 Hue Spread, P6 Detail, P7 Thickness/Glow
static const char* vizCommon = R"GLSL(
vec2 vizP(vec2 uv) { float pulse = 1.0 + uBass * P3 * 0.6; return (aspectUv(uv) - vec2(P1, P2) * vec2(uRes.x / uRes.y, 1.0) * 0.5) / max(P0 * pulse, 1e-3); }
vec3 vizCol(float t) { return hsv2rgb(vec3(fract(P4 + t * P5 + uTime * 0.02), 0.85, 1.0)); }
)GLSL";

static const char* vizBodies[numVizTypes] = {
// 0 Spectrum Bars
R"GLSL(
vec4 gen(vec2 uv) {
    vec2 p = vizP(uv) + vec2(0.5, 0.25);
    if (p.x < 0.0 || p.x > 1.0 || p.y < 0.0) return vec4(0);
    float n = floor(P6); float i = floor(p.x * n); float x = (i + 0.5) / n;
    float h = spec(x) * 0.6;
    float gap = step(P7 * 0.5, fract(p.x * n));
    float on = step(p.y, h) * gap;
    vec3 c = vizCol(x) * (0.6 + p.y * 1.4);
    return vec4(c * on, on);
})GLSL",
// 1 Mirror Bars
R"GLSL(
vec4 gen(vec2 uv) {
    vec2 p = vizP(uv) + vec2(0.5, 0.0);
    if (p.x < 0.0 || p.x > 1.0) return vec4(0);
    float n = floor(P6); float i = floor(p.x * n); float x = abs((i + 0.5) / n - 0.5) * 2.0;
    float h = spec(x) * 0.35;
    float gap = step(P7 * 0.5, fract(p.x * n));
    float on = step(abs(p.y), h) * gap;
    vec3 c = vizCol(x) * (0.5 + abs(p.y) * 3.0);
    return vec4(c * on, on);
})GLSL",
// 2 Circle Bars (with input image in the middle)
R"GLSL(
vec4 gen(vec2 uv) {
    vec2 p = vizP(uv); float r = length(p); float a = atan(p.x, p.y);
    float n = floor(P6); float x = fract(a / TAU + 0.5); float bi = floor(x * n) / n;
    float s = spec(abs(bi - 0.5) * 2.0);
    float inner = 0.22;
    float on = step(inner, r) * step(r, inner + 0.02 + s * 0.3) * step(P7 * 0.5, fract(x * n));
    vec3 c = vizCol(bi) * on;
    float disc = smoothstep(inner - 0.005, inner - 0.01, r);
    vec3 img = texture(uIn, p / (inner * 2.0) + 0.5).rgb;
    float ring = smoothstep(0.006, 0.0, abs(r - inner + 0.004));
    vec3 col = c + img * disc + ring * vizCol(0.0);
    return vec4(col, max(on, max(disc, ring)));
})GLSL",
// 3 Circle Wave
R"GLSL(
vec4 gen(vec2 uv) {
    vec2 p = vizP(uv); float r = length(p); float a = atan(p.y, p.x);
    float x = fract(a / TAU + 0.5); float xs = abs(x - 0.5) * 2.0;
    float w = 0.25 + wave(xs) * 0.08 + spec(xs) * 0.1;
    float d = abs(r - w);
    float th = max(P7, 0.05) * 0.01;
    float line = smoothstep(th * 2.0, th * 0.5, d);
    float glow = exp(-d * 60.0) * 0.4;
    vec3 c = vizCol(xs);
    return vec4(c * (line + glow), clamp(line + glow, 0.0, 1.0));
})GLSL",
// 4 Waveform Line
R"GLSL(
vec4 gen(vec2 uv) {
    vec2 p = vizP(uv) + vec2(0.5, 0.0);
    if (p.x < 0.0 || p.x > 1.0) return vec4(0);
    float y = wave(p.x) * 0.25;
    float th = max(P7, 0.05) * 0.008;
    float d = abs(p.y - y);
    float line = smoothstep(th * 2.0, th * 0.5, d) + exp(-d * 80.0) * 0.3;
    return vec4(vizCol(p.x) * line, clamp(line, 0.0, 1.0));
})GLSL",
// 5 Spectrum Fill
R"GLSL(
vec4 gen(vec2 uv) {
    vec2 p = vizP(uv) + vec2(0.5, 0.25);
    if (p.x < 0.0 || p.x > 1.0 || p.y < 0.0) return vec4(0);
    float h = spec(p.x) * 0.6;
    float edge = smoothstep(0.012, 0.0, abs(p.y - h));
    float fill = step(p.y, h) * (0.25 + 0.5 * p.y / max(h, 1e-3)) * P7;
    float a = clamp(edge + fill, 0.0, 1.0);
    return vec4(vizCol(p.x) * a, a);
})GLSL",
// 6 Particles
R"GLSL(
vec4 gen(vec2 uv) {
    vec2 p = vizP(uv); vec3 col = vec3(0); float a = 0.0;
    float n = floor(P6);
    for (int i = 0; i < 160; i++) { if (float(i) >= n) break; float fi = float(i);
        float h = hash11(fi * 1.7);
        float life = fract(uTime * (0.15 + h * 0.3) * (1.0 + uBass * P3) + h);
        float ang = hash11(fi * 3.1) * TAU;
        vec2 pos = vec2(cos(ang), sin(ang)) * (0.2 + life * 0.8);
        float sz = (0.004 + h * 0.01) * (1.0 + uKick) * max(P7, 0.1);
        float d = length(p - pos);
        float s = smoothstep(sz, 0.0, d) * (1.0 - life);
        col += vizCol(h) * s; a += s; }
    return vec4(col, clamp(a, 0.0, 1.0));
})GLSL",
// 7 Pulse Image
R"GLSL(
vec4 gen(vec2 uv) {
    vec2 p = vizP(uv); float r = length(p);
    float rad = 0.3;
    float m = smoothstep(rad, rad - 0.004, r);
    vec3 img = texture(uIn, p / (rad * 2.0) + 0.5).rgb;
    float ring = smoothstep(0.01 * max(P7, 0.2), 0.0, abs(r - rad - 0.01 - uKick * 0.02));
    vec3 col = img * m + vizCol(0.0) * ring * (0.6 + uBass);
    return vec4(col, max(m, ring));
})GLSL",
// 8 Peak Bars (with falling caps)
R"GLSL(
vec4 gen(vec2 uv) {
    vec2 p = vizP(uv) + vec2(0.5, 0.25);
    if (p.x < 0.0 || p.x > 1.0 || p.y < 0.0) return vec4(0);
    float n = floor(P6); float i = floor(p.x * n); float x = (i + 0.5) / n;
    float h = spec(x) * 0.6, pk = peak(x) * 0.6;
    float gap = step(P7 * 0.5, fract(p.x * n));
    float seg = step(0.25, fract(p.y * 60.0));
    float on = step(p.y, h) * gap * seg;
    float cap = step(abs(p.y - pk - 0.01), 0.004) * gap;
    vec3 c = mix(vizCol(0.0), vizCol(1.0), p.y / 0.6);
    return vec4(c * on + vec3(1) * cap, max(on, cap));
})GLSL",
// 9 Polygon Ring
R"GLSL(
vec4 gen(vec2 uv) {
    vec2 p = vizP(uv); float r = length(p); float a = atan(p.y, p.x) + uTime * 0.2;
    float sides = max(floor(P6 / 8.0), 3.0);
    float poly = cos(PI / sides) / cos(mod(a, TAU / sides) - PI / sides);
    float x = fract(a / TAU); float xs = abs(x - 0.5) * 2.0;
    float target = 0.28 * poly * (1.0 + spec(xs) * 0.5);
    float d = abs(r - target);
    float line = smoothstep(0.004 * max(P7, 0.2) * 3.0, 0.0, d) + exp(-d * 50.0) * 0.3;
    return vec4(vizCol(xs) * line, clamp(line, 0.0, 1.0));
})GLSL",
// 10 Dual Spectrum (horizontal, from center outwards, top & bottom)
R"GLSL(
vec4 gen(vec2 uv) {
    vec2 p = vizP(uv);
    float x = abs(p.x) * 1.6; if (x > 1.0) return vec4(0);
    float n = floor(P6); float bx = (floor(x * n) + 0.5) / n;
    float h = spec(bx) * 0.4;
    float gap = step(P7 * 0.5, fract(x * n));
    float on = step(abs(p.y), h) * gap * step(0.004, abs(p.y));
    vec3 c = vizCol(bx) * (0.4 + abs(p.y) * 3.0);
    return vec4(c * on, on);
})GLSL",
// 11 Glow Orb
R"GLSL(
vec4 gen(vec2 uv) {
    vec2 p = vizP(uv); float r = length(p); float a = atan(p.y, p.x);
    float core = exp(-r * r * 40.0 / (0.3 + uBass * P3)) * 1.5;
    float rays = pow(max(0.0, sin(a * floor(max(P6 / 8.0, 2.0)) + uTime)), 8.0) * exp(-r * 4.0) * (0.3 + uMid) * P7;
    float halo = exp(-abs(r - 0.25 - uKick * 0.05) * 40.0) * 0.6;
    float v = core + rays + halo;
    return vec4(vizCol(r) * v, clamp(v, 0.0, 1.0));
})GLSL",
};

juce::String VizLibrary::glsl (int type)
{
    type = juce::jlimit (0, numVizTypes - 1, type);
    return juce::String (vizCommon) + vizBodies[type];
}
} // namespace moshdeck
