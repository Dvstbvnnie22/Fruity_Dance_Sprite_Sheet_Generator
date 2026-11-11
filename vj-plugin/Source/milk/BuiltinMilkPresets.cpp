#include "MilkPreset.h"

// Original presets written for MoshDeck in the MilkDrop .milk text format.
// Users can additionally point the Milk browser at any folder of .milk presets.

namespace moshdeck::milk
{
namespace
{
struct Builtin { const char* name; const char* text; };

const Builtin builtins[] = {
{ "Liquid Mirror", R"MILK(
fDecay=0.96
fWarpAnimSpeed=0.8
fWarpScale=1.6
fZoomExponent=1.2
nWaveMode=0
fWaveAlpha=0.9
fWaveScale=1.4
bAdditiveWaves=1
bWaveThick=1
fVideoEchoZoom=1.003
fVideoEchoAlpha=0.5
nVideoEchoOrientation=1
zoom=1.01
warp=0.9
per_frame_1=wave_r = 0.5 + 0.5*sin(time*0.61);
per_frame_2=wave_g = 0.5 + 0.5*sin(time*0.73 + 2);
per_frame_3=wave_b = 0.5 + 0.5*sin(time*0.47 + 4);
per_frame_4=zoom = 1.0 + 0.04*bass_att;
per_frame_5=rot = 0.02*sin(time*0.3);
per_pixel_1=warp = warp + rad*0.4*mid;
)MILK" },

{ "Bass Tunnel", R"MILK(
fDecay=0.94
fZoomExponent=2.0
nWaveMode=0
fWaveAlpha=1.0
fWaveScale=1.0
bAdditiveWaves=1
bWaveThick=1
warp=0.2
per_frame_1=zoom = 1.02 + 0.12*bass;
per_frame_2=rot = 0.03*sin(time*0.5) + 0.05*treb_att;
per_frame_3=wave_r = 0.6 + 0.4*sin(time); wave_g = 0.3; wave_b = 0.9;
per_frame_4=ob_size = 0.01; ob_r = wave_b; ob_g = 0; ob_b = wave_r; ob_a = 0.3*bass;
per_pixel_1=zoom = zoom + 0.05*sin(ang*6 + time*2)*rad;
)MILK" },

{ "Spiral Galaxy", R"MILK(
fDecay=0.985
fWarpAnimSpeed=1.2
nWaveMode=5
fWaveAlpha=0.7
fWaveScale=1.2
bAdditiveWaves=1
bWaveDots=1
zoom=0.99
per_frame_1=rot = 0.04 + 0.03*bass;
per_frame_2=wave_r = 0.5 + 0.5*sin(time*1.1); wave_g = 0.5 + 0.5*sin(time*0.9 + 1); wave_b = 1;
per_frame_3=q1 = 0.5 + 0.25*sin(time*0.2);
per_pixel_1=rot = rot + 0.06*(1 - rad);
per_pixel_2=zoom = zoom - 0.02*rad*q1;
)MILK" },

{ "Kaleid Bloom", R"MILK(
fDecay=0.97
nWaveMode=1
fWaveAlpha=0.8
fWaveScale=1.5
bAdditiveWaves=1
bWaveThick=1
fVideoEchoAlpha=0.5
fVideoEchoZoom=1.0
nVideoEchoOrientation=3
zoom=1.0
per_frame_1=zoom = 1.0 + 0.03*sin(time*0.7) + 0.05*bass_att;
per_frame_2=rot = 0.01*sin(time*0.37);
per_frame_3=wave_r = abs(sin(time*0.5)); wave_g = abs(sin(time*0.3 + 1)); wave_b = abs(sin(time*0.7 + 2));
per_pixel_1=dx = 0.005*sin(y*20 + time*2)*mid;
per_pixel_2=dy = 0.005*cos(x*20 + time*1.7)*mid;
)MILK" },

{ "Plasma Drift", R"MILK(
fDecay=0.99
fWarpAnimSpeed=2.0
fWarpScale=0.8
nWaveMode=4
fWaveAlpha=0.6
bAdditiveWaves=1
warp=2.0
zoom=1.0
per_frame_1=warp = 1.5 + 1.5*mid_att;
per_frame_2=wave_r = 0.5 + 0.5*sin(time*2.1); wave_g = 0.5 + 0.5*cos(time*1.3); wave_b = 0.6;
per_frame_3=dx = 0.003*sin(time*0.4); dy = 0.003*cos(time*0.31);
per_pixel_1=zoom = 1 + 0.02*sin(rad*8 - time*3);
)MILK" },

{ "Shockwave", R"MILK(
fDecay=0.92
fZoomExponent=1.0
nWaveMode=0
fWaveAlpha=1.0
fWaveScale=2.0
bAdditiveWaves=1
bWaveThick=1
bBrighten=0
per_frame_1=zoom = 1.0 + 0.25*bass*bass;
per_frame_2=wave_r = 1; wave_g = 0.5*treb; wave_b = 0.2 + 0.8*mid;
per_frame_3=ib_size = 0.005; ib_r = 1; ib_g = 1; ib_b = 1; ib_a = above(bass, 1.2)*0.6;
per_pixel_1=zoom = zoom - 0.1*rad*bass;
)MILK" },

{ "Rotating Cubes", R"MILK(
fDecay=0.95
nWaveMode=6
fWaveAlpha=0.5
zoom=1.02
shapecode_0_enabled=1
shapecode_0_sides=4
shapecode_0_additive=1
shapecode_0_thick=1
shapecode_0_num_inst=6
shapecode_0_rad=0.12
shapecode_0_r=1
shapecode_0_g=0.2
shapecode_0_b=0.6
shapecode_0_a=0.8
shapecode_0_r2=0.1
shapecode_0_g2=0.4
shapecode_0_b2=1
shapecode_0_a2=0.2
shapecode_0_border_a=0.6
shape_0_per_frame1=ang = time*(0.5 + instance*0.1) + instance;
shape_0_per_frame2=x = 0.5 + 0.3*cos(time*0.4 + instance*1.047);
shape_0_per_frame3=y = 0.5 + 0.3*sin(time*0.4 + instance*1.047);
shape_0_per_frame4=rad = 0.06 + 0.06*bass;
per_frame_1=rot = 0.01;
per_frame_2=zoom = 1.01 + 0.03*bass_att;
)MILK" },

{ "Neon Rain", R"MILK(
fDecay=0.93
nWaveMode=4
fWaveAlpha=0.9
fWaveScale=0.8
bAdditiveWaves=1
bWaveThick=1
zoom=1.0
warp=0.0
dy=-0.01
per_frame_1=dy = -0.008 - 0.02*bass;
per_frame_2=wave_r = 0.1; wave_g = 0.8 + 0.2*sin(time); wave_b = 1;
per_frame_3=wave_y = 0.1 + 0.8*abs(sin(time*0.25));
per_pixel_1=dx = 0.003*sin(y*30 + time*4);
)MILK" },

{ "Fire Vortex", R"MILK(
fDecay=0.975
fWarpScale=2.0
nWaveMode=3
fWaveAlpha=1
fWaveScale=1.6
bAdditiveWaves=1
bMaximizeWaveColor=1
zoom=0.98
per_frame_1=wave_r = 1; wave_g = 0.35 + 0.3*sin(time*3); wave_b = 0.05;
per_frame_2=rot = -0.03 - 0.04*bass_att;
per_frame_3=cy = 0.6;
per_pixel_1=dy = -0.006*(1 - y);
per_pixel_2=zoom = zoom + 0.03*sin(ang*3 + time);
)MILK" },

{ "Echo Chamber", R"MILK(
fDecay=0.98
nWaveMode=7
fWaveAlpha=0.8
fWaveParam=0.5
bAdditiveWaves=1
fVideoEchoZoom=0.98
fVideoEchoAlpha=0.7
nVideoEchoOrientation=0
zoom=1.03
per_frame_1=wave_mystery = sin(time*0.4);
per_frame_2=wave_r = 0.5 + 0.5*sin(time*1.7); wave_g = 0.5 + 0.5*sin(time*2.3); wave_b = 0.5 + 0.5*sin(time*2.9);
per_frame_3=echo_zoom = 0.98 + 0.04*bass;
per_frame_4=rot = 0.01*sin(time*0.2);
)MILK" },

{ "Digital Ripple", R"MILK(
fDecay=0.96
nWaveMode=0
fWaveAlpha=0.6
bAdditiveWaves=1
warp=0
per_frame_1=wave_r = 0; wave_g = 1; wave_b = 0.7;
per_frame_2=q1 = bass*0.02;
per_pixel_1=zoom = 1 + q1*sin(rad*40 - time*6);
per_pixel_2=rot = 0.005*sin(rad*20 - time*3);
)MILK" },

{ "Starburst", R"MILK(
fDecay=0.88
nWaveMode=5
fWaveAlpha=0.7
fWaveScale=1.6
bAdditiveWaves=1
bWaveDots=1
fZoomExponent=3
per_frame_1=zoom = 1.05 + 0.15*bass;
per_frame_2=rot = 0.1*sin(time*0.1);
per_frame_3=wave_r = 1; wave_g = 0.9; wave_b = 0.6 + 0.4*sin(time);
per_frame_4=mv_a = 0.2*treb; mv_r = 1; mv_g = 0.6; mv_b = 0.2; mv_l = 2;
)MILK" },

{ "Aurora", R"MILK(
fDecay=0.985
fWarpAnimSpeed=0.5
fWarpScale=3.0
nWaveMode=6
fWaveAlpha=0.5
fWaveParam=0.2
bAdditiveWaves=1
bWaveThick=1
warp=1.5
dy=0.002
per_frame_1=wave_r = 0.1 + 0.2*sin(time*0.3); wave_g = 0.9; wave_b = 0.6 + 0.3*sin(time*0.5);
per_frame_2=wave_mystery = 0.3*sin(time*0.2);
per_frame_3=wave_y = 0.6 + 0.2*sin(time*0.15);
per_pixel_1=dy = 0.003*(1 + mid)*sin(x*6 + time);
)MILK" },

{ "Hypnotic Squares", R"MILK(
fDecay=0.97
nWaveMode=4
fWaveAlpha=0.0
zoom=0.97
shapecode_0_enabled=1
shapecode_0_sides=4
shapecode_0_additive=0
shapecode_0_thick=1
shapecode_0_rad=0.4
shapecode_0_a=0.0
shapecode_0_a2=0.0
shapecode_0_border_r=1
shapecode_0_border_g=1
shapecode_0_border_b=1
shapecode_0_border_a=1
shape_0_per_frame1=ang = time*0.3; rad = 0.35 + 0.1*bass;
shape_0_per_frame2=border_r = 0.5 + 0.5*sin(time); border_g = 0.5 + 0.5*sin(time + 2); border_b = 0.5 + 0.5*sin(time + 4);
per_frame_1=rot = 0.05*sin(time*0.25);
)MILK" },

{ "Ocean Floor", R"MILK(
fDecay=0.99
fWarpAnimSpeed=0.4
fWarpScale=1.2
nWaveMode=4
fWaveAlpha=0.4
bAdditiveWaves=1
warp=1.2
per_frame_1=wave_r = 0.1; wave_g = 0.5; wave_b = 1;
per_frame_2=wave_y = 0.5 + 0.3*sin(time*0.3);
per_frame_3=zoom = 1.0 + 0.01*sin(time*0.5);
per_pixel_1=dx = 0.004*sin(y*10 + time*0.8);
per_pixel_2=dy = 0.004*sin(x*8 + time*0.6);
)MILK" },

{ "Strobe Grid", R"MILK(
fDecay=0.85
nWaveMode=1
fWaveAlpha=1
fWaveScale=1
bAdditiveWaves=1
bWaveThick=1
bSolarize=0
per_frame_1=mv_x = 24; mv_y = 18; mv_l = 0.5; mv_a = 0.6*bass; mv_r = 0.3; mv_g = 1; mv_b = 1;
per_frame_2=wave_r = above(bass, 1.0); wave_g = 1; wave_b = 1;
per_frame_3=zoom = 1.0 + 0.02*treb;
)MILK" },

{ "Wormhole", R"MILK(
fDecay=0.96
fZoomExponent=4
nWaveMode=0
fWaveAlpha=0.9
fWaveScale=0.8
bAdditiveWaves=1
per_frame_1=zoom = 1.15 + 0.1*bass;
per_frame_2=rot = 0.08*sin(time*0.2);
per_frame_3=cx = 0.5 + 0.1*sin(time*0.33); cy = 0.5 + 0.1*cos(time*0.29);
per_frame_4=wave_r = 0.6 + 0.4*sin(time*0.9); wave_g = 0.2; wave_b = 0.6 + 0.4*cos(time*0.7);
per_pixel_1=rot = rot + 0.1*(1 - rad);
)MILK" },

{ "Chrome Flower", R"MILK(
fDecay=0.97
nWaveMode=2
fWaveAlpha=0.9
fWaveScale=1.8
bAdditiveWaves=1
bWaveDots=1
bBrighten=0
fVideoEchoAlpha=0.4
nVideoEchoOrientation=2
per_frame_1=wave_r = 0.8; wave_g = 0.8; wave_b = 1;
per_frame_2=zoom = 1.0 + 0.03*bass_att;
per_pixel_1=zoom = zoom + 0.04*sin(ang*5 + time)*(1 - rad);
per_pixel_2=rot = 0.02*cos(ang*5 - time*0.5);
)MILK" },

{ "Lava Flow", R"MILK(
fDecay=0.985
fWarpAnimSpeed=0.6
fWarpScale=2.5
nWaveMode=4
fWaveAlpha=0.7
bAdditiveWaves=1
bWaveThick=1
warp=2.5
dy=-0.004
per_frame_1=wave_r = 1; wave_g = 0.3 + 0.2*bass; wave_b = 0;
per_frame_2=wave_y = 0.85;
per_frame_3=dy = -0.004 - 0.01*bass_att;
per_pixel_1=dx = 0.005*sin(y*7 + time);
)MILK" },

{ "Sonar", R"MILK(
fDecay=0.95
nWaveMode=0
fWaveAlpha=0.0
zoom=1.01
shapecode_0_enabled=1
shapecode_0_sides=64
shapecode_0_additive=1
shapecode_0_rad=0.05
shapecode_0_a=0
shapecode_0_a2=0
shapecode_0_border_r=0.2
shapecode_0_border_g=1
shapecode_0_border_b=0.5
shapecode_0_border_a=1
shape_0_per_frame1=rad = 0.05 + 0.4*bass*bass;
shape_0_per_frame2=border_a = 0.5 + 0.5*bass;
per_frame_1=zoom = 1.02 + 0.02*bass;
)MILK" },

{ "Glitch Spin", R"MILK(
fDecay=0.93
nWaveMode=1
fWaveAlpha=1
fWaveScale=1.2
bAdditiveWaves=0
bWaveThick=1
bInvert=0
per_frame_1=rot = if(above(bass, 1.1), 0.3*sin(time*20), 0.01);
per_frame_2=zoom = if(above(treb, 1.0), 0.9, 1.02);
per_frame_3=wave_r = rand(2); wave_g = rand(2); wave_b = 1;
per_frame_4=dx = 0.02*(rand(3) - 1)*above(mid, 1.1);
)MILK" },

{ "Sunset Waves", R"MILK(
fDecay=0.975
nWaveMode=6
fWaveAlpha=0.7
fWaveParam=0
bAdditiveWaves=1
bWaveThick=1
fGammaAdj=1.4
wavecode_0_enabled=1
wavecode_0_samples=256
wavecode_0_additive=1
wavecode_0_thick=1
wavecode_0_r=1
wavecode_0_g=0.5
wavecode_0_b=0.1
wavecode_0_a=0.9
wave_0_per_point1=x = sample;
wave_0_per_point2=y = 0.35 + value1*0.15 + 0.05*sin(sample*12 + time*2);
wave_0_per_point3=g = 0.3 + 0.4*sample;
per_frame_1=dy = 0.003;
per_frame_2=wave_r = 1; wave_g = 0.4; wave_b = 0.6;
)MILK" },

{ "Particle Fountain", R"MILK(
fDecay=0.94
nWaveMode=2
fWaveAlpha=0
zoom=1
dy=0.01
wavecode_0_enabled=1
wavecode_0_samples=300
wavecode_0_usedots=1
wavecode_0_additive=1
wavecode_0_r=1
wavecode_0_g=1
wavecode_0_b=1
wave_0_per_point1=t = sample*6.283*7 + time*0.5;
wave_0_per_point2=x = 0.5 + sin(t)*sample*0.4*(0.6 + bass*0.4);
wave_0_per_point3=y = 0.1 + sample*0.8 + value1*0.05;
wave_0_per_point4=r = 0.5 + 0.5*sin(sample*10 + time); g = 0.5 + 0.5*sin(sample*7); b = 1;
per_frame_1=dy = 0.004 + 0.01*bass;
)MILK" },

{ "Infinite Zoom", R"MILK(
fDecay=0.98
fZoomExponent=1
nWaveMode=0
fWaveAlpha=0.8
fWaveScale=0.6
bAdditiveWaves=1
per_frame_1=zoom = 0.95 - 0.05*bass;
per_frame_2=rot = 0.02;
per_frame_3=wave_r = 0.5 + 0.5*sin(time*0.8); wave_g = 0.5 + 0.5*sin(time*1.1 + 2); wave_b = 0.5 + 0.5*sin(time*1.4 + 4);
per_frame_4=ob_size = 0.02; ob_r = wave_g; ob_g = wave_b; ob_b = wave_r; ob_a = 0.8;
)MILK" },

{ "Mosh Feed", R"MILK(
fDecay=1.0
fWarpAnimSpeed=1.0
fWarpScale=1.0
nWaveMode=4
fWaveAlpha=0.0
warp=0.5
zoom=1.0
per_frame_1=zoom = 1.0 + 0.01*bass;
per_pixel_1=dx = 0.01*sin(y*13 + time*1.3)*mid;
per_pixel_2=dy = 0.01*cos(x*11 + time*1.1)*bass;
)MILK" },

{ "Electric Lotus", R"MILK(
fDecay=0.96
nWaveMode=0
fWaveAlpha=0.9
fWaveScale=1.3
bAdditiveWaves=1
bWaveThick=1
fVideoEchoAlpha=0.5
fVideoEchoZoom=1.02
nVideoEchoOrientation=1
shapecode_0_enabled=1
shapecode_0_sides=6
shapecode_0_additive=1
shapecode_0_num_inst=12
shapecode_0_rad=0.08
shapecode_0_r=0.9
shapecode_0_g=0.2
shapecode_0_b=1
shapecode_0_a=0.4
shapecode_0_a2=0
shapecode_0_border_a=0.5
shape_0_per_frame1=a1 = instance/num_inst*6.283 + time*0.3;
shape_0_per_frame2=x = 0.5 + 0.25*cos(a1)*(1 + 0.3*bass);
shape_0_per_frame3=y = 0.5 + 0.25*sin(a1)*(1 + 0.3*bass);
shape_0_per_frame4=ang = a1; r = 0.5 + 0.5*sin(a1 + time);
per_frame_1=rot = 0.015;
per_frame_2=wave_r = 1; wave_g = 0.6; wave_b = 1;
)MILK" },

{ "Deep Space", R"MILK(
fDecay=0.97
nWaveMode=5
fWaveAlpha=0.5
fWaveScale=1.5
bAdditiveWaves=1
bWaveDots=1
fGammaAdj=1.15
zoom=1.04
per_frame_1=zoom = 1.04 + 0.04*bass;
per_frame_2=wave_r = 0.6; wave_g = 0.7; wave_b = 1;
per_frame_3=rot = 0.005*sin(time*0.1);
per_pixel_1=zoom = zoom + 0.02*rad;
)MILK" },

{ "Dancer Outline", R"MILK(
fDecay=0.9
nWaveMode=4
fWaveAlpha=0.3
bAdditiveWaves=1
zoom=1.02
bDarkenCenter=1
per_frame_1=zoom = 1.015 + 0.04*bass;
per_frame_2=rot = 0.01*sin(time);
per_frame_3=wave_r = 1; wave_g = 1; wave_b = 1;
)MILK" },
};
} // namespace

const juce::StringArray& BuiltinMilkPresets::names()
{
    static const juce::StringArray n = []
    {
        juce::StringArray a;
        for (auto& b : builtins) a.add (b.name);
        return a;
    }();
    return n;
}

juce::String BuiltinMilkPresets::text (const juce::String& name)
{
    for (auto& b : builtins)
        if (name == b.name)
            return b.text;
    return builtins[0].text;
}
} // namespace moshdeck::milk
