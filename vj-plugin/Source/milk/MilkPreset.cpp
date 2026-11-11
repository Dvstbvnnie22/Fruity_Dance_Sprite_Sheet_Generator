#include "MilkPreset.h"
#include <cmath>
#include <cstring>
#include <limits>

namespace moshdeck::milk
{
std::string milkKeyToVar (const juce::String& keyIn)
{
    static const std::map<std::string, std::string> m = {
        { "fgammaadj", "gamma" }, { "fdecay", "decay" }, { "fvideoechozoom", "echo_zoom" }, { "fvideoechoalpha", "echo_alpha" },
        { "nvideoechoorientation", "echo_orient" }, { "nwavemode", "wave_mode" }, { "badditivewaves", "additivewave" },
        { "bwavedots", "wave_dots" }, { "bwavethick", "wave_thick" }, { "bmodwavealphabyvolume", "modwavealphabyvolume" },
        { "bmaximizewavecolor", "wave_brighten" }, { "btexwrap", "wrap" }, { "bdarkencenter", "darken_center" },
        { "bredbluestereo", "red_blue" }, { "bbrighten", "brighten" }, { "bdarken", "darken" }, { "bsolarize", "solarize" },
        { "binvert", "invert" }, { "fwavealpha", "wave_a" }, { "fwavescale", "wave_scale" }, { "fwavesmoothing", "wave_smoothing" },
        { "fwaveparam", "wave_mystery" }, { "fmodwavealphastart", "modwavealphastart" }, { "fmodwavealphaend", "modwavealphaend" },
        { "fwarpanimspeed", "warpanimspeed" }, { "fwarpscale", "warpscale" }, { "fzoomexponent", "zoomexp" }, { "fshader", "fshader" },
        { "nmotionvectorsx", "mv_x" }, { "nmotionvectorsy", "mv_y" }, { "frating", "rating" },
        // custom shape / wave keys
        { "num_inst", "instances" }, { "bspectrum", "spectrum" }, { "busedots", "usedots" }, { "bdrawthick", "thick" },
        { "badditive", "additive" } };
    const auto key = keyIn.toLowerCase().toStdString();
    auto it = m.find (key);
    return it != m.end() ? it->second : key;
}

static void appendCode (std::map<int, std::string>& dst, int index, const juce::String& code)
{
    auto& s = dst[index];
    s += code.toStdString();
    s += " ";
}

static std::string joined (const std::map<int, std::string>& m)
{
    std::string s;
    for (auto& kv : m) s += kv.second;
    return s;
}

MilkPresetData MilkPresetData::parse (const juce::String& text, const juce::String& name)
{
    MilkPresetData d;
    d.name = name;
    std::map<int, std::string> init, frame, pixel;
    std::array<std::map<int, std::string>, 4> sInit, sFrame, wInit, wFrame, wPoint;

    juce::StringArray lines;
    lines.addLines (text);
    for (auto line : lines)
    {
        line = line.trim();
        if (line.isEmpty() || line.startsWithChar ('[') || line.startsWith ("//"))
            continue;
        const int eq = line.indexOfChar ('=');
        if (eq <= 0) continue;
        const auto key = line.substring (0, eq).trim();
        const auto val = line.substring (eq + 1);
        const auto lk = key.toLowerCase();

        auto trailingInt = [] (const juce::String& k, const juce::String& prefix)
        { return k.substring (prefix.length()).getIntValue(); };

        if (lk.startsWith ("per_frame_init_"))      appendCode (init, trailingInt (lk, "per_frame_init_"), val);
        else if (lk.startsWith ("per_frame_"))      appendCode (frame, trailingInt (lk, "per_frame_"), val);
        else if (lk.startsWith ("per_pixel_"))      appendCode (pixel, trailingInt (lk, "per_pixel_"), val);
        else if (lk.startsWith ("warp_") || lk.startsWith ("comp_")) d.hasPixelShaders = true;
        else if (lk.startsWith ("shapecode_"))
        {
            const int i = lk.substring (10).getIntValue();
            if (i >= 0 && i < 4)
            {
                const auto sub = lk.substring (10).fromFirstOccurrenceOf ("_", false, false);
                d.shapes[(size_t) i].base[milkKeyToVar (sub)] = val.getDoubleValue();
            }
        }
        else if (lk.startsWith ("wavecode_"))
        {
            const int i = lk.substring (9).getIntValue();
            if (i >= 0 && i < 4)
            {
                const auto sub = lk.substring (9).fromFirstOccurrenceOf ("_", false, false);
                d.waves[(size_t) i].base[milkKeyToVar (sub)] = val.getDoubleValue();
            }
        }
        else if (lk.startsWith ("shape_") || lk.startsWith ("wave_") )
        {
            const bool isShape = lk.startsWith ("shape_");
            const auto rest = lk.substring (isShape ? 6 : 5);
            const int i = rest.getIntValue();
            const auto what = rest.fromFirstOccurrenceOf ("_", false, false); // init1 / per_frame1 / per_point1
            if (! juce::CharacterFunctions::isDigit (rest[0]) || i < 0 || i > 3 || what.isEmpty())
            {
                d.base[milkKeyToVar (key)] = val.getDoubleValue(); // e.g. wave_r=...
                continue;
            }
            auto num = [] (const juce::String& w, const char* p) { return w.substring ((int) std::strlen (p)).getIntValue(); };
            if (what.startsWith ("init"))            appendCode (isShape ? sInit[(size_t) i] : wInit[(size_t) i], num (what, "init"), val);
            else if (what.startsWith ("per_frame"))  appendCode (isShape ? sFrame[(size_t) i] : wFrame[(size_t) i], num (what, "per_frame"), val);
            else if (what.startsWith ("per_point"))  appendCode (wPoint[(size_t) i], num (what, "per_point"), val);
        }
        else
            d.base[milkKeyToVar (key)] = val.getDoubleValue();
    }
    d.perFrameInit = joined (init);
    d.perFrame = joined (frame);
    d.perPixel = joined (pixel);
    for (size_t i = 0; i < 4; ++i)
    {
        auto& s = d.shapes[i];
        s.init = joined (sInit[i]); s.perFrame = joined (sFrame[i]);
        s.enabled = s.base.count ("enabled") && s.base["enabled"] != 0.0;
        auto& w = d.waves[i];
        w.init = joined (wInit[i]); w.perFrame = joined (wFrame[i]); w.perPoint = joined (wPoint[i]);
        w.enabled = w.base.count ("enabled") && w.base["enabled"] != 0.0;
    }
    return d;
}

MilkPresetData MilkPresetData::load (const juce::File& f)
{
    return parse (f.loadFileAsString(), f.getFileNameWithoutExtension());
}

//==============================================================================
static const std::map<std::string, double>& frameDefaults()
{
    static const std::map<std::string, double> d = {
        { "zoom", 1 }, { "rot", 0 }, { "cx", 0.5 }, { "cy", 0.5 }, { "dx", 0 }, { "dy", 0 }, { "warp", 1 }, { "sx", 1 }, { "sy", 1 },
        { "decay", 0.98 }, { "gamma", 1 }, { "echo_zoom", 1 }, { "echo_alpha", 0 }, { "echo_orient", 0 },
        { "wave_mode", 0 }, { "wave_a", 0.8 }, { "wave_scale", 1 }, { "wave_smoothing", 0.75 }, { "wave_mystery", 0 },
        { "wave_r", 1 }, { "wave_g", 1 }, { "wave_b", 1 }, { "wave_x", 0.5 }, { "wave_y", 0.5 },
        { "warpanimspeed", 1 }, { "warpscale", 1 }, { "zoomexp", 1 },
        { "ob_size", 0.01 }, { "ob_r", 0 }, { "ob_g", 0 }, { "ob_b", 0 }, { "ob_a", 0 },
        { "ib_size", 0.01 }, { "ib_r", 0.25 }, { "ib_g", 0.25 }, { "ib_b", 0.25 }, { "ib_a", 0 },
        { "mv_x", 12 }, { "mv_y", 9 }, { "mv_dx", 0 }, { "mv_dy", 0 }, { "mv_l", 0.9 }, { "mv_r", 1 }, { "mv_g", 1 }, { "mv_b", 1 }, { "mv_a", 0 },
        { "modwavealphastart", 0.75 }, { "modwavealphaend", 0.95 }, { "wrap", 1 } };
    return d;
}

static const std::map<std::string, double>& shapeDefaults()
{
    static const std::map<std::string, double> d = {
        { "sides", 4 }, { "additive", 0 }, { "thick", 0 }, { "textured", 0 }, { "instances", 1 }, { "x", 0.5 }, { "y", 0.5 },
        { "rad", 0.1 }, { "ang", 0 }, { "tex_ang", 0 }, { "tex_zoom", 1 }, { "r", 1 }, { "g", 0 }, { "b", 0 }, { "a", 1 },
        { "r2", 0 }, { "g2", 1 }, { "b2", 0 }, { "a2", 0 }, { "border_r", 1 }, { "border_g", 1 }, { "border_b", 1 }, { "border_a", 0.1 } };
    return d;
}

static const std::map<std::string, double>& waveDefaults()
{
    static const std::map<std::string, double> d = {
        { "samples", 512 }, { "sep", 0 }, { "spectrum", 0 }, { "usedots", 0 }, { "thick", 0 }, { "additive", 0 },
        { "scaling", 1 }, { "smoothing", 0.5 }, { "r", 1 }, { "g", 1 }, { "b", 1 }, { "a", 1 } };
    return d;
}

static void applyMap (VarTable& v, const std::map<std::string, double>& m)
{
    for (auto& kv : m) v.set (kv.first, kv.second);
}

static const char* qName (int i)
{
    static std::array<std::string, 32> names = [] { std::array<std::string, 32> a; for (int k = 0; k < 32; ++k) a[(size_t) k] = "q" + std::to_string (k + 1); return a; }();
    return names[(size_t) i].c_str();
}

bool MilkState::load (const MilkPresetData& d, juce::String* errors)
{
    *this = MilkState();
    name = d.name;
    hasPixelShaders = d.hasPixelShaders;
    vars.gmegabuf = &gmegabuf;

    baseVals = frameDefaults();
    for (auto& kv : d.base) baseVals[kv.first] = kv.second;
    applyMap (vars, baseVals);

    juce::String err, all;
    auto compile = [&] (Program& p, const std::string& src, VarTable& vt, const char* what)
    {
        if (src.empty()) return;
        if (! p.compile (src, vt, &err))
            all << what << ": " << err << "\n";
    };
    compile (progInit, d.perFrameInit, vars, "per_frame_init");
    compile (progFrame, d.perFrame, vars, "per_frame");
    compile (progPixel, d.perPixel, vars, "per_pixel");

    for (size_t i = 0; i < 4; ++i)
    {
        auto& sp = shapeProgs[i];
        sp.enabled = d.shapes[i].enabled;
        sp.base = shapeDefaults();
        for (auto& kv : d.shapes[i].base) sp.base[kv.first] = kv.second;
        sp.vars.gmegabuf = &gmegabuf;
        applyMap (sp.vars, sp.base);
        compile (sp.init, d.shapes[i].init, sp.vars, "shape init");
        compile (sp.frame, d.shapes[i].perFrame, sp.vars, "shape per_frame");

        auto& wp = waveProgs[i];
        wp.enabled = d.waves[i].enabled;
        wp.base = waveDefaults();
        for (auto& kv : d.waves[i].base) wp.base[kv.first] = kv.second;
        wp.vars.gmegabuf = &gmegabuf;
        applyMap (wp.vars, wp.base);
        compile (wp.init, d.waves[i].init, wp.vars, "wave init");
        compile (wp.frame, d.waves[i].perFrame, wp.vars, "wave per_frame");
        compile (wp.point, d.waves[i].perPoint, wp.vars, "wave per_point");
    }

    vX = vars.slot ("x"); vY = vars.slot ("y"); vRad = vars.slot ("rad"); vAng = vars.slot ("ang");
    vZoom = vars.slot ("zoom"); vZoomExp = vars.slot ("zoomexp"); vRot = vars.slot ("rot"); vWarp = vars.slot ("warp");
    vCx = vars.slot ("cx"); vCy = vars.slot ("cy"); vDx = vars.slot ("dx"); vDy = vars.slot ("dy"); vSx = vars.slot ("sx"); vSy = vars.slot ("sy");
    uv.assign ((size_t) ((meshW + 1) * (meshH + 1) * 2), 0.5f);

    if (errors) *errors = all;
    return all.isEmpty();
}

void MilkState::runFrame (const FrameInputs& in, const float* wave512, const float* spectrum512)
{
    auto setCommon = [&] (VarTable& v)
    {
        v.set ("time", in.time); v.set ("fps", in.fps); v.set ("frame", in.frame); v.set ("progress", 0.5);
        v.set ("bass", in.bass); v.set ("mid", in.mid); v.set ("treb", in.treb);
        v.set ("bass_att", in.bassAtt); v.set ("mid_att", in.midAtt); v.set ("treb_att", in.trebAtt);
        v.set ("meshx", meshW); v.set ("meshy", meshH); v.set ("aspectx", in.aspectX); v.set ("aspecty", in.aspectY);
        v.set ("pixelsx", in.pixelsX); v.set ("pixelsy", in.pixelsY);
    };

    // reset per-frame variables to the preset's base values each frame (MilkDrop semantics)
    applyMap (vars, baseVals);
    setCommon (vars);
    if (! initDone)
    {
        progInit.run (vars);
        for (int i = 0; i < 32; ++i) qInit[(size_t) i] = vars.get (qName (i));
        initDone = true;
    }
    for (int i = 0; i < 32; ++i) vars.set (qName (i), qInit[(size_t) i]);
    progFrame.run (vars);

    // user offsets
    vars[vZoom] += in.zoomOffset;
    vars[vRot]  += in.rotOffset;
    vars[vWarp] *= in.warpScale;
    vars.set ("decay", juce::jlimit (0.0, 1.0, vars.get ("decay") + in.decayOffset));

    std::array<double, 32> q {};
    for (int i = 0; i < 32; ++i) q[(size_t) i] = vars.get (qName (i));

    computeMesh (in);
    buildMainWave (in, wave512);

    // ---- custom shapes
    for (size_t si = 0; si < 4; ++si)
    {
        auto& sp = shapeProgs[si];
        auto& out = shapes[si];
        out.enabled = sp.enabled;
        out.inst.clear();
        if (! sp.enabled) continue;
        const int n = juce::jlimit (1, 1024, (int) sp.base["instances"]);
        out.instances = n;
        if (! sp.inited)
        {
            setCommon (sp.vars);
            for (int i = 0; i < 32; ++i) sp.vars.set (qName (i), q[(size_t) i]);
            sp.init.run (sp.vars);
            sp.inited = true;
        }
        for (int k = 0; k < n; ++k)
        {
            applyMap (sp.vars, sp.base);
            setCommon (sp.vars);
            sp.vars.set ("instance", k);
            sp.vars.set ("num_inst", n);
            for (int i = 0; i < 32; ++i) sp.vars.set (qName (i), q[(size_t) i]);
            sp.frame.run (sp.vars);
            auto g = [&] (const char* nm) { return (float) sp.vars.get (nm); };
            ShapeOut::Inst s {};
            s.x = g ("x"); s.y = g ("y"); s.rad = g ("rad"); s.ang = g ("ang");
            s.r = g ("r"); s.g = g ("g"); s.b = g ("b"); s.a = g ("a");
            s.r2 = g ("r2"); s.g2 = g ("g2"); s.b2 = g ("b2"); s.a2 = g ("a2");
            s.br = g ("border_r"); s.bg = g ("border_g"); s.bb = g ("border_b"); s.ba = g ("border_a");
            s.sides = juce::jlimit (3, 100, (int) g ("sides"));
            s.additive = g ("additive") != 0; s.thick = g ("thick") != 0; s.textured = g ("textured") != 0;
            s.texZoom = g ("tex_zoom"); s.texAng = g ("tex_ang");
            out.inst.push_back (s);
        }
    }

    // ---- custom waves
    for (size_t wi = 0; wi < 4; ++wi)
    {
        auto& wp = waveProgs[wi];
        auto& out = waves[wi];
        out.enabled = wp.enabled;
        out.xy.clear(); out.rgba.clear();
        if (! wp.enabled) continue;
        if (! wp.inited)
        {
            setCommon (wp.vars);
            for (int i = 0; i < 32; ++i) wp.vars.set (qName (i), q[(size_t) i]);
            wp.init.run (wp.vars);
            wp.inited = true;
        }
        applyMap (wp.vars, wp.base);
        setCommon (wp.vars);
        for (int i = 0; i < 32; ++i) wp.vars.set (qName (i), q[(size_t) i]);
        wp.frame.run (wp.vars);

        const int samples = juce::jlimit (2, 512, (int) wp.vars.get ("samples"));
        const bool spectrum = wp.vars.get ("spectrum") != 0;
        const float scaling = (float) wp.vars.get ("scaling");
        const int sep = juce::jlimit (0, 256, (int) wp.vars.get ("sep"));
        out.dots = wp.vars.get ("usedots") != 0;
        out.thick = wp.vars.get ("thick") != 0;
        out.additive = wp.vars.get ("additive") != 0;
        const float fr = (float) wp.vars.get ("r"), fg = (float) wp.vars.get ("g"), fb = (float) wp.vars.get ("b"), fa = (float) wp.vars.get ("a");
        const int sSample = wp.vars.slot ("sample"), sV1 = wp.vars.slot ("value1"), sV2 = wp.vars.slot ("value2");
        const int sx = wp.vars.slot ("x"), sy = wp.vars.slot ("y"), sr = wp.vars.slot ("r"), sg = wp.vars.slot ("g"), sb = wp.vars.slot ("b"), sa = wp.vars.slot ("a");
        for (int i = 0; i < samples; ++i)
        {
            const float t = (float) i / (float) (samples - 1);
            const int idx = juce::jlimit (0, 511, (int) (t * 511.0f));
            const int idx2 = juce::jlimit (0, 511, idx + sep);
            const float v1 = spectrum ? spectrum512[idx] * scaling : wave512[idx] * scaling;
            const float v2 = spectrum ? spectrum512[idx2] * scaling : wave512[idx2] * scaling;
            wp.vars[sSample] = t; wp.vars[sV1] = v1; wp.vars[sV2] = v2;
            wp.vars[sx] = 0.5 + v1 * 0.5; wp.vars[sy] = 0.5 + v2 * 0.5;
            wp.vars[sr] = fr; wp.vars[sg] = fg; wp.vars[sb] = fb; wp.vars[sa] = fa;
            wp.point.run (wp.vars);
            out.xy.push_back ((float) wp.vars[sx]); out.xy.push_back ((float) wp.vars[sy]);
            out.rgba.push_back ((float) wp.vars[sr]); out.rgba.push_back ((float) wp.vars[sg]);
            out.rgba.push_back ((float) wp.vars[sb]); out.rgba.push_back ((float) wp.vars[sa]);
        }
    }
}

void MilkState::computeMesh (const FrameInputs& in)
{
    const double warpTime = in.time * vars.get ("warpanimspeed");
    const double warpScaleInv = 1.0 / juce::jmax (0.01, vars.get ("warpscale"));
    const double f0 = 11.68 + 4.0 * std::cos (warpTime * 1.413 + 10);
    const double f1 = 8.77 + 3.0 * std::cos (warpTime * 1.113 + 7);
    const double f2 = 10.54 + 3.0 * std::cos (warpTime * 1.233 + 3);
    const double f3 = 11.49 + 4.0 * std::cos (warpTime * 0.933 + 5);
    const double ax = in.aspectX, ay = in.aspectY;

    // save per-frame values; per-pixel code may modify them per vertex
    const double pf[10] = { vars[vZoom], vars[vZoomExp], vars[vRot], vars[vWarp], vars[vCx], vars[vCy], vars[vDx], vars[vDy], vars[vSx], vars[vSy] };
    const bool hasPixel = ! progPixel.empty();

    size_t o = 0;
    for (int gy = 0; gy <= meshH; ++gy)
        for (int gx = 0; gx <= meshW; ++gx)
        {
            const double fx = (double) gx / meshW * 2.0 - 1.0;
            const double fy = (double) gy / meshH * 2.0 - 1.0; // up
            double zoom = pf[0], zoomexp = pf[1], rot = pf[2], warp = pf[3], cx = pf[4], cy = pf[5], dx = pf[6], dy = pf[7], sx = pf[8], sy = pf[9];
            const double rad = std::sqrt (fx * fx * ax * ax + fy * fy * ay * ay);
            if (hasPixel)
            {
                vars[vX] = fx * 0.5 * ax + 0.5;
                vars[vY] = -fy * 0.5 * ay + 0.5;
                vars[vRad] = rad;
                vars[vAng] = std::atan2 (fy * ay, fx * ax);
                vars[vZoom] = zoom; vars[vZoomExp] = zoomexp; vars[vRot] = rot; vars[vWarp] = warp;
                vars[vCx] = cx; vars[vCy] = cy; vars[vDx] = dx; vars[vDy] = dy; vars[vSx] = sx; vars[vSy] = sy;
                progPixel.run (vars);
                zoom = vars[vZoom]; zoomexp = vars[vZoomExp]; rot = vars[vRot]; warp = vars[vWarp];
                cx = vars[vCx]; cy = vars[vCy]; dx = vars[vDx]; dy = vars[vDy]; sx = vars[vSx]; sy = vars[vSy];
            }
            const double zoom2 = std::pow (juce::jmax (1.0e-4, zoom), std::pow (juce::jmax (1.0e-4, zoomexp), rad * 2.0 - 1.0));
            const double zinv = 1.0 / zoom2;
            double u = fx * ax * 0.5 * zinv + 0.5;
            double v = -fy * ay * 0.5 * zinv + 0.5;
            u = (u - cx) / (sx != 0 ? sx : 1.0) + cx;
            v = (v - cy) / (sy != 0 ? sy : 1.0) + cy;
            if (warp != 0.0)
            {
                u += warp * 0.0035 * std::sin (warpTime * 0.333 + warpScaleInv * (fx * f0 - fy * f3));
                v += warp * 0.0035 * std::cos (warpTime * 0.375 - warpScaleInv * (fx * f2 + fy * f1));
                u += warp * 0.0035 * std::cos (warpTime * 0.753 - warpScaleInv * (fx * f1 - fy * f2));
                v += warp * 0.0035 * std::sin (warpTime * 0.825 + warpScaleInv * (fx * f0 + fy * f3));
            }
            const double u2 = u - cx, v2 = v - cy;
            const double cr = std::cos (rot), sr = std::sin (rot);
            u = u2 * cr - v2 * sr + cx;
            v = u2 * sr + v2 * cr + cy;
            u -= dx; v -= dy;
            u = (u - 0.5) / ax + 0.5;
            v = (v - 0.5) / ay + 0.5;
            uv[o++] = (float) u;
            uv[o++] = (float) (1.0 - v); // to GL (y up)
        }
    // restore per-frame values so later reads (waves, borders) see them
    vars[vZoom] = pf[0]; vars[vZoomExp] = pf[1]; vars[vRot] = pf[2]; vars[vWarp] = pf[3]; vars[vCx] = pf[4];
    vars[vCy] = pf[5]; vars[vDx] = pf[6]; vars[vDy] = pf[7]; vars[vSx] = pf[8]; vars[vSy] = pf[9];
}

void MilkState::buildMainWave (const FrameInputs& in, const float* w)
{
    auto& out = mainWave;
    out.xy.clear(); out.rgba.clear();
    const int mode = (int) vars.get ("wave_mode") % 8;
    const float scale = (float) (vars.get ("wave_scale") * in.waveScale);
    const float wx = (float) vars.get ("wave_x"), wy = (float) vars.get ("wave_y");
    const float myst = (float) vars.get ("wave_mystery");
    float r = (float) vars.get ("wave_r"), g = (float) vars.get ("wave_g"), b = (float) vars.get ("wave_b");
    float a = (float) vars.get ("wave_a");

    if (vars.get ("wave_brighten") != 0)
    {
        const float m = juce::jmax (r, juce::jmax (g, b));
        if (m > 0.0f) { r /= m; g /= m; b /= m; }
    }
    if (vars.get ("modwavealphabyvolume") != 0)
    {
        const float vol = (in.bass + in.mid + in.treb) / 3.0f;
        const float s0 = (float) vars.get ("modwavealphastart"), s1 = (float) vars.get ("modwavealphaend");
        a *= juce::jlimit (0.0f, 1.0f, (vol - s0) / juce::jmax (0.01f, s1 - s0));
    }
    if (mode == 3) a *= juce::jlimit (0.0f, 1.0f, in.treb * in.treb);
    if (a <= 0.001f) { out.enabled = false; return; }
    out.enabled = true;
    out.dots = vars.get ("wave_dots") != 0 || mode == 2 || mode == 3;
    out.thick = vars.get ("wave_thick") != 0;
    out.additive = vars.get ("additivewave") != 0;

    // smoothing
    std::array<float, 512> s {};
    const float sm = juce::jlimit (0.0f, 0.95f, (float) vars.get ("wave_smoothing"));
    float acc = w[0];
    for (int i = 0; i < 512; ++i) { acc = acc * sm + w[i] * (1.0f - sm); s[(size_t) i] = acc; }

    const float aspect = in.aspectX > 0 ? in.aspectY / in.aspectX : 1.0f; // >1 for landscape
    auto push = [&] (float x, float y) { out.xy.push_back (x); out.xy.push_back (y); out.rgba.insert (out.rgba.end(), { r, g, b, a }); };
    const float yUp = 1.0f - wy; // MilkDrop wave_y is measured from the top

    switch (mode)
    {
        case 0: // circle
        {
            const int n = 128;
            for (int i = 0; i <= n; ++i)
            {
                const float t = (float) (i % n) / n;
                const float rad = 0.25f + 0.1f * myst + s[(size_t) ((i % n) * 3)] * 0.2f * scale;
                const float ang = t * juce::MathConstants<float>::twoPi + (float) in.time * 0.2f;
                push (wx + rad * std::cos (ang) / aspect, yUp + rad * std::sin (ang));
            }
            break;
        }
        case 1: case 2: case 3: case 5: // X/Y oscilloscope variants
        {
            const float rot = mode == 5 ? (float) in.time * 0.3f : 0.0f;
            for (int i = 0; i < 480; ++i)
            {
                float x = s[(size_t) i] * 0.45f * scale, y = s[(size_t) (i + 32)] * 0.45f * scale;
                if (mode == 5) { const float xr = x * std::cos (rot) - y * std::sin (rot); y = x * std::sin (rot) + y * std::cos (rot); x = xr; }
                push (wx + x / aspect, yUp + y);
            }
            break;
        }
        case 4: // horizontal
        {
            for (int i = 0; i < 512; i += 2)
            {
                const float t = (float) i / 511.0f;
                push (t, yUp + s[(size_t) i] * 0.25f * scale);
            }
            break;
        }
        case 6: case 7: // angled line(s)
        {
            const float ang = myst * juce::MathConstants<float>::pi * 0.5f;
            const int lines = mode == 7 ? 2 : 1;
            for (int L = 0; L < lines; ++L)
            {
                const float off = mode == 7 ? (L == 0 ? -0.1f : 0.1f) * (1.0f + myst) : 0.0f;
                for (int i = 0; i < 512; i += 2)
                {
                    const float t = (float) i / 511.0f - 0.5f;
                    const float d = s[(size_t) i] * 0.2f * scale + off;
                    push (wx + (t * std::cos (ang) - d * std::sin (ang)) / aspect, yUp + t * std::sin (ang) + d * std::cos (ang));
                }
                if (L == 0 && lines == 2)
                    push (std::numeric_limits<float>::quiet_NaN(), 0.0f); // strip break
            }
            break;
        }
        default: break;
    }
}
} // namespace moshdeck::milk
