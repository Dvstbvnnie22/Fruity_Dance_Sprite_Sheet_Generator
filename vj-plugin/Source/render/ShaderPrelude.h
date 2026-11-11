#pragma once

namespace moshdeck::shaders
{
/** Vertex shader shared by every full-screen pass. */
inline const char* fullscreenVS = R"GLSL(#version 330 core
layout(location = 0) in vec2 aPos;
out vec2 vUv;
void main() { vUv = aPos * 0.5 + 0.5; gl_Position = vec4(aPos, 0.0, 1.0); }
)GLSL";

/**
    Prelude for all MoshDeck effect / generator / visualizer shaders.
    Textures:
      uIn      current input (effect chain input, or generator base picture)
      uPrev    this pass's own previous output (feedback / datamosh state)
      uPrevIn  previous frame's input (for motion estimation)
      uDeckA / uDeckB (+ uDeckAPrev / uDeckBPrev previous frames)
      uBelow   composite of layers underneath
      uAudio   512x2 Shadertoy-style audio texture (row0 FFT, row1 waveform 0..1)
      uSpec    128x2 log spectrum (row0 smoothed, row1 peaks)
*/
inline const char* prelude = R"GLSL(#version 330 core
in vec2 vUv;
out vec4 fragColor;
uniform sampler2D uIn, uPrev, uPrevIn, uDeckA, uDeckB, uDeckAPrev, uDeckBPrev, uBelow, uAudio, uSpec;
uniform vec2  uRes;
uniform float uTime, uBeat, uBar, uBpm, uBeats;
uniform float uBands[5];
uniform float uLevel, uKick, uSnare, uHat, uPulse, uRand, uMix;
uniform float uTrigBeat, uTrigBar, uTrigKick;
uniform int   uFrame, uLocalFrame;
uniform float uP[8];
#define P0 uP[0]
#define P1 uP[1]
#define P2 uP[2]
#define P3 uP[3]
#define P4 uP[4]
#define P5 uP[5]
#define P6 uP[6]
#define P7 uP[7]
#define uSub    uBands[0]
#define uBass   uBands[1]
#define uMid    uBands[2]
#define uHiMid  uBands[3]
#define uTreble uBands[4]
#define PI  3.14159265359
#define TAU 6.28318530718

float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }
float hash11(float p) { p = fract(p * 0.1031); p *= p + 33.33; p *= p + p; return fract(p); }
float hash21(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }
vec2  hash22(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973)); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.xx + p3.yz) * p3.zy); }
float noise(vec2 p) {
    vec2 i = floor(p), f = fract(p); vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash21(i), hash21(i + vec2(1, 0)), u.x), mix(hash21(i + vec2(0, 1)), hash21(i + vec2(1, 1)), u.x), u.y);
}
float fbm(vec2 p) { float v = 0.0, a = 0.5; for (int i = 0; i < 5; i++) { v += a * noise(p); p = p * 2.03 + 17.1; a *= 0.5; } return v; }
mat2  rot2(float a) { float c = cos(a), s = sin(a); return mat2(c, -s, s, c); }
vec3 hsv2rgb(vec3 c) { vec3 p = abs(fract(c.xxx + vec3(0.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0); return c.z * mix(vec3(1.0), clamp(p - 1.0, 0.0, 1.0), c.y); }
vec3 rgb2hsv(vec3 c) {
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
    float d = q.x - min(q.w, q.y); float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}
vec3 palette(float t, vec3 a, vec3 b, vec3 c, vec3 d) { return a + b * cos(TAU * (c * t + d)); }
vec3 rainbow(float t) { return palette(t, vec3(0.5), vec3(0.5), vec3(1.0), vec3(0.0, 0.33, 0.67)); }
float spec(float x)  { return texture(uSpec, vec2(clamp(x, 0.0, 1.0), 0.25)).r; }
float peak(float x)  { return texture(uSpec, vec2(clamp(x, 0.0, 1.0), 0.75)).r; }
float fft(float x)   { return texture(uAudio, vec2(clamp(x, 0.0, 1.0), 0.25)).r; }
float wave(float x)  { return texture(uAudio, vec2(clamp(x, 0.0, 1.0), 0.75)).r * 2.0 - 1.0; }
vec2  aspectUv(vec2 uv) { return (uv - 0.5) * vec2(uRes.x / uRes.y, 1.0); }
vec2  blockUv(vec2 uv, float px) { vec2 b = max(px, 1.0) / uRes; return (floor(uv / b) + 0.5) * b; }
bool  refreshNow(float mode) {
    int m = int(mode + 0.5);
    return uLocalFrame < 2 || (m == 1 && uTrigBeat > 0.5) || (m == 2 && uTrigBar > 0.5) || (m == 3 && uTrigKick > 0.5);
}
/** Gradient-based optical flow between `cur` and `prev` at scale s (pixels). Returns UV displacement. */
vec2 flowBetween(sampler2D cur, sampler2D prev, vec2 uv, float s) {
    vec2 px = max(s, 1.0) / uRes;
    float c  = luma(texture(cur, uv).rgb), p = luma(texture(prev, uv).rgb);
    float gx = luma(texture(cur, uv + vec2(px.x, 0)).rgb) - luma(texture(cur, uv - vec2(px.x, 0)).rgb)
             + luma(texture(prev, uv + vec2(px.x, 0)).rgb) - luma(texture(prev, uv - vec2(px.x, 0)).rgb);
    float gy = luma(texture(cur, uv + vec2(0, px.y)).rgb) - luma(texture(cur, uv - vec2(0, px.y)).rgb)
             + luma(texture(prev, uv + vec2(0, px.y)).rgb) - luma(texture(prev, uv - vec2(0, px.y)).rgb);
    vec2 g = vec2(gx, gy) * 0.25;
    float it = c - p;
    vec2 f = -it * g / (dot(g, g) + 0.0015);
    return clamp(f, -3.0, 3.0) * px;
}
vec2 flowAt(vec2 uv, float s) { return flowBetween(uIn, uPrevIn, uv, s); }
)GLSL";
} // namespace moshdeck::shaders
