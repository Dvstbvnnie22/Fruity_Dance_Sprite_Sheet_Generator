#include "ShadertoyImporter.h"
#include <map>

namespace moshdeck
{
using Kind = ShadertoyProject::ChannelKind;

static Kind kindFromName (juce::String n)
{
    n = n.trim().toLowerCase();
    if (n == "input" || n == "base" || n == "texture" || n == "video") return Kind::Input;
    if (n == "decka" || n == "deck a" || n == "a") return Kind::DeckA;
    if (n == "deckb" || n == "deck b" || n == "b") return Kind::DeckB;
    if (n == "below" || n == "layers") return Kind::Below;
    if (n == "audio" || n == "music" || n == "sound" || n == "mic") return Kind::Audio;
    if (n == "self" || n == "feedback") return Kind::Self;
    if (n == "noise") return Kind::Noise;
    if (n.startsWith ("buffer")) return Kind::Buffer;
    return Kind::None;
}

juce::String ShadertoyProject::defaultShader()
{
    return R"GLSL(// Paste any Shadertoy "Image" shader here, or import a Shadertoy JSON export.
// Channel defaults: iChannel0 = layer input, iChannel1 = Deck B, iChannel2 = audio (FFT/wave), iChannel3 = layers below
// Remap with directives, e.g.   // @iChannel0: audio     (input, deckA, deckB, below, audio, self, noise)
// MoshDeck extras: iParam0..3 (modulatable), iBass iMid iTreble iLevel iKick iBeat iBpm
void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 uv = fragCoord / iResolution.xy;
    vec2 p = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;
    float fft = texture(iChannel2, vec2(abs(p.x), 0.25)).x;
    float r = length(p);
    vec3 col = 0.5 + 0.5 * cos(iTime + r * 8.0 - iBass * 4.0 + vec3(0, 2, 4));
    col *= smoothstep(0.02, 0.0, abs(r - 0.25 - fft * 0.2));
    vec3 base = texture(iChannel0, uv + p * fft * 0.05).rgb;
    fragColor = vec4(base * iParam0 + col, 1.0);
}
)GLSL";
}

static void applyDirectives (const juce::String& code, std::array<ShadertoyProject::Channel, 4>& ch)
{
    juce::StringArray lines;
    lines.addLines (code);
    for (auto& l : lines)
    {
        const auto t = l.trim();
        if (! t.startsWith ("//")) continue;
        const auto body = t.substring (2).trim();
        if (! body.startsWithIgnoreCase ("@iChannel")) continue;
        const int idx = body.substring (9, 10).getIntValue();
        if (idx < 0 || idx > 3) continue;
        const auto what = body.fromFirstOccurrenceOf (":", false, false).trim();
        auto& c = ch[(size_t) idx];
        c.kind = kindFromName (what);
        if (c.kind == Kind::Buffer)
        {
            const auto letter = what.getLastCharacter();
            c.buffer = juce::jlimit (0, 3, (int) (juce::CharacterFunctions::toUpperCase (letter) - 'A'));
        }
        if (what.containsIgnoreCase ("repeat")) c.repeat = true;
        if (what.containsIgnoreCase ("nearest")) c.linear = false;
    }
}

static int bufferIndexFromName (const juce::String& n)
{
    const auto t = n.trim().toUpperCase();
    if (t == "BUFFER A" || t == "BUF A") return 0;
    if (t == "BUFFER B" || t == "BUF B") return 1;
    if (t == "BUFFER C" || t == "BUF C") return 2;
    if (t == "BUFFER D" || t == "BUF D") return 3;
    return -1;
}

static int bufferIndexFromId (const juce::var& id)
{
    // well-known Shadertoy buffer output ids (string and numeric forms)
    const auto s = id.toString();
    if (s == "4dXGR8" || s == "257") return 0;
    if (s == "XsXGR8" || s == "258") return 1;
    if (s == "4sXGR8" || s == "259") return 2;
    if (s == "XdfGR8" || s == "260") return 3;
    return -1;
}

ShadertoyProject ShadertoyProject::parse (const juce::String& text, juce::String& error)
{
    ShadertoyProject proj;
    const auto trimmed = text.trimStart();
    if (! (trimmed.startsWithChar ('{') || trimmed.startsWithChar ('[')))
    {
        Pass p;
        p.name = "Image";
        p.code = text;
        p.channels[0].kind = Kind::Input;
        p.channels[1].kind = Kind::DeckB;
        p.channels[2].kind = Kind::Audio;
        p.channels[3].kind = Kind::Below;
        applyDirectives (text, p.channels);
        if (! text.contains ("mainImage"))
            error = "No mainImage() function found";
        proj.name = "Shadertoy";
        proj.passes.push_back (p);
        return proj;
    }

    juce::var root;
    const auto res = juce::JSON::parse (text, root);
    if (res.failed()) { error = "JSON parse error: " + res.getErrorMessage(); return proj; }
    if (root.isArray() && root.size() > 0) root = root[0];
    juce::var shader = root.hasProperty ("Shader") ? root["Shader"] : root;
    if (shader.isArray() && shader.size() > 0) shader = shader[0];
    proj.name = shader["info"]["name"].toString();
    auto rp = shader["renderpass"];
    if (! rp.isArray()) { error = "No renderpass array in JSON"; return proj; }

    // first pass: map output ids to buffer slots
    std::map<juce::String, int> outIdToBuffer;
    for (int i = 0; i < rp.size(); ++i)
    {
        auto pass = rp[i];
        if (pass["type"].toString() != "buffer") continue;
        int b = bufferIndexFromName (pass["name"].toString());
        auto outs = pass["outputs"];
        if (outs.isArray() && outs.size() > 0)
        {
            const auto id = outs[0]["id"];
            if (b < 0) b = bufferIndexFromId (id);
            if (b >= 0) outIdToBuffer[id.toString()] = b;
        }
    }

    std::vector<Pass> buffers (4), image;
    std::vector<bool> haveBuffer (4, false);
    for (int i = 0; i < rp.size(); ++i)
    {
        auto pass = rp[i];
        const auto type = pass["type"].toString();
        if (type == "common") { proj.common = pass["code"].toString(); continue; }
        if (type != "image" && type != "buffer") continue; // sound / cubemap passes unsupported

        Pass p;
        p.name = pass["name"].toString();
        p.code = pass["code"].toString();
        if (type == "buffer")
        {
            int b = bufferIndexFromName (p.name);
            auto outs = pass["outputs"];
            if (b < 0 && outs.isArray() && outs.size() > 0) b = bufferIndexFromId (outs[0]["id"]);
            if (b < 0) continue;
            p.output = b;
        }
        auto ins = pass["inputs"];
        if (ins.isArray())
            for (int k = 0; k < ins.size(); ++k)
            {
                auto in = ins[k];
                const int chn = (int) in["channel"];
                if (chn < 0 || chn > 3) continue;
                auto& c = p.channels[(size_t) chn];
                const auto ctype = in.hasProperty ("ctype") ? in["ctype"].toString() : in["type"].toString();
                auto sampler = in["sampler"];
                c.linear = sampler["filter"].toString() != "nearest";
                c.repeat = sampler["wrap"].toString() == "repeat";
                if (ctype == "buffer")
                {
                    c.kind = Kind::Buffer;
                    auto it = outIdToBuffer.find (in["id"].toString());
                    c.buffer = it != outIdToBuffer.end() ? it->second : bufferIndexFromId (in["id"]);
                    if (c.buffer < 0)
                    {
                        const auto src = in["src"].toString(); // ".../buffer00.png"
                        const int n = src.fromLastOccurrenceOf ("buffer", false, false).getIntValue();
                        c.buffer = juce::jlimit (0, 3, n);
                    }
                    if (c.buffer == p.output) c.kind = Kind::Self;
                }
                else if (ctype == "music" || ctype == "musicstream" || ctype == "mic") c.kind = Kind::Audio;
                else if (ctype == "texture" || ctype == "video" || ctype == "webcam")
                {
                    const auto src = in["src"].toString();
                    c.kind = src.containsIgnoreCase ("noise") ? Kind::Noise : (chn == 1 ? Kind::DeckB : Kind::Input);
                }
                else c.kind = Kind::None;
            }
        applyDirectives (p.code, p.channels);
        if (p.output >= 0) { buffers[(size_t) p.output] = p; haveBuffer[(size_t) p.output] = true; }
        else image.push_back (p);
    }
    for (int b = 0; b < 4; ++b)
        if (haveBuffer[(size_t) b]) proj.passes.push_back (buffers[(size_t) b]);
    if (image.empty()) { error = "No Image pass found"; return proj; }
    proj.passes.push_back (image.front());
    return proj;
}

juce::String ShadertoyProject::buildFragmentSource (const Pass& pass) const
{
    juce::String s;
    s << "#version 330 core\n"
         "out vec4 moshFragColor;\n"
         "uniform vec3 iResolution;\n"
         "uniform float iTime, iTimeDelta, iFrameRate, iSampleRate;\n"
         "uniform int iFrame;\n"
         "uniform vec4 iMouse, iDate;\n"
         "uniform float iChannelTime[4];\n"
         "uniform vec3 iChannelResolution[4];\n"
         "uniform sampler2D iChannel0, iChannel1, iChannel2, iChannel3;\n"
         "uniform float iParam0, iParam1, iParam2, iParam3;\n"
         "uniform float iBass, iMid, iTreble, iLevel, iKick, iBeat, iBpm;\n"
         "#define HW_PERFORMANCE 1\n"
         "#line 1\n";
    s << common << "\n#line 1\n" << pass.code << "\n";
    s << "void main() { vec4 c = vec4(0.0, 0.0, 0.0, 1.0); mainImage(c, gl_FragCoord.xy); moshFragColor = c; }\n";
    return s;
}

juce::String ShadertoyProject::fetchFromWeb (const juce::String& idOrUrl, const juce::String& apiKey, juce::String& error)
{
    juce::String id = idOrUrl.trim();
    if (id.contains ("/view/")) id = id.fromLastOccurrenceOf ("/view/", false, false).upToFirstOccurrenceOf ("?", false, false);
    id = id.removeCharacters ("/ ");
    if (id.isEmpty() || apiKey.isEmpty()) { error = "Need a shader ID and a Shadertoy API key"; return {}; }
    juce::URL url ("https://www.shadertoy.com/api/v1/shaders/" + id + "?key=" + apiKey);
    auto stream = url.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress).withConnectionTimeoutMs (10000));
    if (stream == nullptr) { error = "Could not connect to shadertoy.com"; return {}; }
    const auto body = stream->readEntireStreamAsString();
    if (body.contains ("\"Error\"")) { error = "Shadertoy API: " + body; return {}; }
    return body;
}
} // namespace moshdeck
