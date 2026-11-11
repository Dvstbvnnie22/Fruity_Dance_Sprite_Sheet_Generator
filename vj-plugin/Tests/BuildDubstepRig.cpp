// Builds rigs/dubstep-machine/dubstep-machine.mdvj with the real Scene API, so the
// file is guaranteed to load in MoshDeck VJ. Usage: MoshDeckRigBuilder <out.mdvj>
#include <juce_core/juce_core.h>
#include "model/Scene.h"

using namespace moshdeck;

int main (int argc, char** argv)
{
    if (argc < 2) { std::cerr << "usage: MoshDeckRigBuilder <out.mdvj>\n"; return 1; }
    Scene s;
    for (auto& l : s.getLayers()) s.removeLayer (l.uid);

    // Macro N (1-based, FL automation "Macro N") -> param, amount (fraction of the param's range)
    auto macro = [&] (int n, const juce::String& dest, float amount, bool bipolar = false, float smooth = 0.0f)
    {
        ModRoute r; r.source = ModSourceType::Macro; r.sourceIndex = n - 1; r.dest = dest; r.bipolar = bipolar; r.smoothing = smooth;
        s.addRoute (r, amount);
    };
    auto audio = [&] (ModSourceType t, int idx, const juce::String& dest, float amount, float smooth = 0.0f)
    {
        ModRoute r; r.source = t; r.sourceIndex = idx; r.dest = dest; r.smoothing = smooth;
        s.addRoute (r, amount);
    };

    // ---- L1: the two main tracks (loop visuals on A, cover art / clips on B)
    const auto deck = s.addLayer (SourceType::DeckMix);
    s.editLayer (deck, [] (Layer& l) { l.name = "Decks A/B"; });
    s.setValue (deck + ".p4", 0.25f);                          // bass pulse zoom
    const auto bloom = s.addEffect (deck, "mosh_bloom");      // DROP datamosh
    s.setValue (deck + "." + bloom + ".mix", 0.0f);
    s.setValue (deck + "." + bloom + ".p3", 2.0f);            // keyframe refresh every bar
    const auto slices = s.addEffect (deck, "mosh_slices");
    s.setValue (deck + "." + slices + ".mix", 0.0f);
    macro (9, deck + ".opacity", -0.6f);                      // ch A fader: 0 = full, up = fade (inverted later via base)
    s.setValue (deck + ".opacity", 1.0f);
    macro (11, deck + "." + bloom + ".mix", 1.0f);            // DROP button
    macro (7, deck + "." + bloom + ".p0", 0.5f, false, 0.6f); // jog A -> motion gain
    macro (16, deck + "." + slices + ".mix", 1.0f);           // FX button -> glitch slices
    audio (ModSourceType::Kick, 0, deck + "." + slices + ".p0", 0.15f);

    // ---- L2: NestDrop-style Milk feedback eating the decks
    const auto milk = s.addLayer (SourceType::Milk);
    s.editLayer (milk, [] (Layer& l) { l.name = "Milk Feedback"; l.milkPreset = "builtin:Bass Tunnel"; l.input = InputRoute::Below; l.blendMode = (int) BlendMode::Screen; });
    s.setValue (milk + ".opacity", 0.85f);
    s.setValue (milk + ".p0", 0.12f);                         // base inject
    s.setValue (milk + ".p1", 2.0f);                          // inject on kick
    s.setValue (milk + ".p7", 32.0f);                         // next preset every 32 beats
    macro (2, milk + ".p4", 0.35f, false, 0.3f);              // bass filter -> warp
    macro (3, milk + ".p0", 0.5f, false, 0.2f);               // bass level (peak ctrl) -> inject
    macro (6, milk + ".p2", 0.4f, true, 0.3f);                // tempo B -> zoom
    const auto kal = s.addEffect (milk, "kaleido");
    s.setValue (milk + "." + kal + ".mix", 0.0f);
    macro (14, milk + "." + kal + ".mix", 1.0f);              // loop encoder A
    macro (14, milk + "." + kal + ".p0", 0.4f);               // ... and more segments
    const auto tf = s.addEffect (milk, "transform");
    macro (8, milk + "." + tf + ".p1", 0.5f, true, 0.6f);     // jog B -> rotate

    // ---- L3: Avee-style circle spectrum with cover art (Deck B) in the middle
    const auto circ = s.addLayer (SourceType::AudioViz, 2);
    s.editLayer (circ, [] (Layer& l) { l.name = "Circle Spectrum"; l.input = InputRoute::DeckB; });
    s.setValue (circ + ".p0", 1.1f);
    s.setValue (circ + ".opacity", 0.0f);
    macro (10, circ + ".opacity", 1.0f);                      // ch B fader
    macro (4, circ + ".p4", 1.0f, false, 0.3f);               // lead filter -> hue
    macro (4, circ + ".p3", 0.4f);                            // ... and bass react

    // ---- L4: particles
    const auto parts = s.addLayer (SourceType::AudioViz, 6);
    s.editLayer (parts, [] (Layer& l) { l.name = "Particles"; l.input = InputRoute::None; });
    s.setValue (parts + ".p0", 1.6f);
    audio (ModSourceType::AudioBand, 1, parts + ".p3", 0.3f, 0.5f);

    // ---- master chain
    const auto hue = s.addEffect ("master", "hue_shift");
    s.setValue ("master." + hue + ".p0", 0.0f);
    macro (15, "master." + hue + ".p0", 1.0f, false, 0.4f);   // browse encoder
    const auto flash = s.addEffect ("master", "beat_flash");
    s.setValue ("master." + flash + ".mix", 0.0f);
    macro (1, "master." + flash + ".mix", 1.0f);              // drum peak controller
    macro (13, "master." + flash + ".mix", 1.0f);             // sync buttons
    const auto rgb = s.addEffect ("master", "rgb_split");
    s.setValue ("master." + rgb + ".p0", 0.002f);
    macro (1, "master." + rgb + ".p0", 0.4f);
    const auto strobe = s.addEffect ("master", "strobe");
    s.setValue ("master." + strobe + ".mix", 0.0f);
    s.setValue ("master." + strobe + ".p0", 2.0f);            // 8th-note strobe
    macro (12, "master." + strobe + ".mix", 1.0f);            // cue buttons
    const auto inv = s.addEffect ("master", "invert");
    s.setValue ("master." + inv + ".mix", 0.0f);
    s.setValue ("master." + inv + ".p1", 1.0f);               // invert on kicks
    macro (11, "master." + inv + ".mix", 1.0f);               // DROP also flips on kicks

    // ---- decks
    macro (5, "DA.speed", 0.15f, true, 0.3f);                 // tempo A -> clip speed
    s.setValue ("DA.beats", 8.0f);                            // loop clips every 2 bars, locked to FL
    s.setValue ("DB.next", 8.0f);                             // deck B playlist: next every 8 bars

    // Fix ch-A fader direction: fader up = more deck. Base 0.4 + macro 0.6.
    for (auto& r : s.getRoutes())
        if (r.dest == deck + ".opacity") s.removeRoute (r.uid);
    s.setValue (deck + ".opacity", 0.4f);
    macro (9, deck + ".opacity", 0.6f);

    juce::ValueTree preset ("MoshDeckPreset");
    preset.appendChild (s.toValueTree(), nullptr);
    juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    auto xml = preset.createXml();
    if (xml == nullptr || ! xml->writeTo (out)) { std::cerr << "write failed\n"; return 1; }

    // verify it loads back
    auto back = juce::XmlDocument::parse (out);
    Scene t;
    t.fromValueTree (juce::ValueTree::fromXml (*back).getChildWithName ("MoshDeckScene"));
    std::cout << "wrote " << out.getFullPathName() << ": layers=" << t.getLayers().size()
              << " routes=" << t.getRoutes().size() << " masterFx=" << t.getEffects ("master").size() << "\n";
    return t.getLayers().size() == 4 && t.getRoutes().size() == s.getRoutes().size() ? 0 : 1;
}
