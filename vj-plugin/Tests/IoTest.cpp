// Spout / NDI loopback test.
//
// Renders a known pattern (top-left green, top-right red, bottom half blue) through the
// headless Broadcaster, sends it out over Spout and NDI, receives it back through the same
// MediaDeck "spout:" / "ndi:" input path the decks use, and checks size, orientation and colour.
//
//   MoshDeckIoTest [--no-spout] [--no-ndi] [--timeout seconds]
//
// Without a Spout-capable OS or an installed NDI runtime the matching part is reported as SKIP.
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_events/juce_events.h>
#include "io/Broadcaster.h"
#include "io/LiveSources.h"
#include "io/NdiApi.h"
#include "io/SpoutIO.h"
#include "media/MediaDeck.h"
#include "model/Scene.h"
#include "render/RenderEngine.h"
#include <iostream>
#include <mutex>

using namespace moshdeck;

struct TestHost : EngineHost
{
    Scene scene;
    MediaDeck decks[2];
    std::atomic<int> deckAdvances { 0 };
    Scene& getScene() override { return scene; }
    MediaDeck& getDeck (int i) override { return decks[juce::jlimit (0, 1, i)]; }
    void buildFrameInput (FrameInput& in) override
    {
        in.mod.beats = juce::Time::getMillisecondCounterHiRes() * 0.001 * 2.0;
        in.mod.bpm = 120.0;
        in.mod.beatsPerBar = 4;
    }
    void getCanvasSize (int& w, int& h) override { w = 640; h = 360; }
    void setLayerStatus (const juce::String&, const juce::String&) override {}
    void onDeckAdvance (int, double) override { ++deckAdvances; }
    void onMilkAdvance (const juce::String&, double) override {}
};

static int failures = 0;
static void report (bool ok, const juce::String& what)
{
    std::cout << (ok ? "PASS  " : "FAIL  ") << what << std::endl;
    if (! ok) ++failures;
}

struct Rgb { int r, g, b; };

/** Pixel at (fx, fy) in [0,1], y measured from the TOP of the image. */
static Rgb sample (const uint8_t* px, int w, int h, bool bgra, bool bottomUp, float fx, float fy)
{
    const int x = juce::jlimit (0, w - 1, (int) (fx * (float) w));
    int y = juce::jlimit (0, h - 1, (int) (fy * (float) h));
    if (bottomUp) y = h - 1 - y;
    const auto* p = px + ((size_t) y * (size_t) w + (size_t) x) * 4;
    return bgra ? Rgb { p[2], p[1], p[0] } : Rgb { p[0], p[1], p[2] };
}

static bool near (Rgb c, Rgb want) { return std::abs (c.r - want.r) < 48 && std::abs (c.g - want.g) < 48 && std::abs (c.b - want.b) < 48; }

static juce::String describe (Rgb c) { return "(" + juce::String (c.r) + "," + juce::String (c.g) + "," + juce::String (c.b) + ")"; }

/** Checks the pattern. Returns an empty string when it matches. */
static juce::String checkPattern (const uint8_t* px, int w, int h, bool bgra, bool bottomUp)
{
    const Rgb tl = sample (px, w, h, bgra, bottomUp, 0.25f, 0.25f);
    const Rgb tr = sample (px, w, h, bgra, bottomUp, 0.75f, 0.25f);
    const Rgb bm = sample (px, w, h, bgra, bottomUp, 0.50f, 0.75f);
    juce::String err;
    if (! near (tl, { 0, 255, 0 })) err << " top-left " << describe (tl) << " should be green";
    if (! near (tr, { 255, 0, 0 })) err << " top-right " << describe (tr) << " should be red";
    if (! near (bm, { 0, 0, 255 })) err << " bottom " << describe (bm) << " should be blue";
    return err.trim();
}

static juce::File writePattern()
{
    juce::Image img (juce::Image::ARGB, 160, 90, true);
    {
        juce::Graphics g (img);
        g.fillAll (juce::Colours::blue);
        g.setColour (juce::Colour (0xffff0000)); g.fillRect (0, 0, 160, 45);
        g.setColour (juce::Colour (0xff00ff00)); g.fillRect (0, 0, 80, 45);
    }
    auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("moshdeck_io_pattern.png");
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
    return f;
}

/** Waits for a deck to deliver a frame from a live source and checks it. */
static void receiveAndCheck (MediaDeck& deck, const juce::String& label, double timeoutS, int wantW, int wantH)
{
    VideoFrame f;
    const double end = juce::Time::getMillisecondCounterHiRes() + timeoutS * 1000.0;
    bool got = false;
    int frames = 0;
    juce::String err;
    while (juce::Time::getMillisecondCounterHiRes() < end)
    {
        if (deck.fetch (f, f.serial))
        {
            got = true;
            ++frames;
            err = checkPattern (f.rgba.data(), f.width, f.height, f.bgra, true); // deck frames are stored bottom-up
            if (err.isEmpty() && frames >= 3) break;
        }
        juce::Thread::sleep (10);
    }
    std::cout << "      " << label << " deck status: " << deck.getStatus() << std::endl;
    report (got, label + " input receives frames");
    if (! got) return;
    report (f.width == wantW && f.height == wantH, label + " frame size " + juce::String (f.width) + "x" + juce::String (f.height)
                                                    + " (expected " + juce::String (wantW) + "x" + juce::String (wantH) + ")");
    report (err.isEmpty(), label + " picture is the right way up and the right colours" + (err.isEmpty() ? juce::String() : ": " + err));
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    bool wantSpout = true, wantNdi = true;
    double timeoutS = 15.0;
    for (int i = 1; i < argc; ++i)
    {
        const juce::String a (argv[i]);
        if (a == "--no-spout") wantSpout = false;
        else if (a == "--no-ndi") wantNdi = false;
        else if (a == "--timeout" && i + 1 < argc) timeoutS = juce::String (argv[++i]).getDoubleValue();
    }
    const juce::String name = "MoshDeckIoTest";

    TestHost host;
    for (auto& l : host.scene.getLayers()) host.scene.removeLayer (l.uid);
    host.scene.addLayer (SourceType::DeckA);
    host.decks[0].load (writePattern().getFullPathName());

    // ---- 1. headless render + readback (always)
    Broadcaster bc (host);
    std::mutex tapMutex;
    std::vector<uint8_t> tapped;
    int tapW = 0, tapH = 0, tapFrames = 0;
    bc.setFrameTap ([&] (const uint8_t* px, int w, int h)
    {
        std::lock_guard<std::mutex> l (tapMutex);
        tapped.assign (px, px + (size_t) w * (size_t) h * 4);
        tapW = w; tapH = h; ++tapFrames;
    });
    {
        const double end = juce::Time::getMillisecondCounterHiRes() + timeoutS * 1000.0;
        juce::String err = "no frames";
        while (juce::Time::getMillisecondCounterHiRes() < end)
        {
            juce::Thread::sleep (50);
            std::lock_guard<std::mutex> l (tapMutex);
            if (tapFrames > 10) { err = checkPattern (tapped.data(), tapW, tapH, true, false); if (err.isEmpty()) break; }
        }
        const auto st = bc.getStatus();
        if (st.render.isNotEmpty() && st.render != "Rendering") std::cout << "      " << st.render << std::endl;
        std::lock_guard<std::mutex> l (tapMutex);
        report (tapFrames > 10, "headless renderer produces frames (" + juce::String (tapFrames) + ")");
        report (tapW == 640 && tapH == 360, "broadcast frame is the canvas size (" + juce::String (tapW) + "x" + juce::String (tapH) + ")");
        report (err.isEmpty(), "broadcast frame is top-row-first BGRA with the right colours" + (err.isEmpty() ? juce::String() : ": " + err));
    }

    // ---- 2. Spout out -> Spout in
    const bool spoutOk = wantSpout && spout::isAvailable();
    // ---- 3. NDI out -> NDI in
    juce::String ndiProblem;
    const bool ndiOk = wantNdi && ndi::api (&ndiProblem) != nullptr;
    if (wantNdi && ! ndiOk) std::cout << "SKIP  NDI: " << ndiProblem << std::endl;
    if (wantSpout && ! spoutOk) std::cout << "SKIP  Spout: not available on this platform" << std::endl;

    Broadcaster::Settings s;
    s.spout = spoutOk; s.spoutName = name;
    s.ndi = ndiOk; s.ndiName = name;
    s.fps = 30;
    bc.setSettings (s);

    if (spoutOk)
    {
        bool listed = false;
        const double end = juce::Time::getMillisecondCounterHiRes() + timeoutS * 1000.0;
        while (! listed && juce::Time::getMillisecondCounterHiRes() < end)
        {
            listed = spout::listSenders().contains (name);
            if (! listed) juce::Thread::sleep (100);
        }
        std::cout << "      broadcaster: " << bc.getStatus().spout << std::endl;
        report (listed, "Spout sender '" + name + "' is visible to other apps");
        MediaDeck rx;
        rx.load (live::spoutPath (name));
        receiveAndCheck (rx, "Spout", timeoutS, 640, 360);
    }

    if (ndiOk)
    {
        ndi::FinderLease lease;
        juce::String full;
        const double end = juce::Time::getMillisecondCounterHiRes() + timeoutS * 1000.0;
        while (full.isEmpty() && juce::Time::getMillisecondCounterHiRes() < end)
        {
            for (auto& n : ndi::Finder::get().getSources())
                if (n.endsWith ("(" + name + ")")) full = n;
            if (full.isEmpty()) juce::Thread::sleep (200);
        }
        std::cout << "      runtime: " << ndi::runtimePath() << std::endl;
        report (full.isNotEmpty(), "NDI source found on the network: " + (full.isNotEmpty() ? full : juce::String ("(none)")));
        if (full.isNotEmpty())
        {
            MediaDeck rx;
            rx.load (live::ndiPath (full));
            receiveAndCheck (rx, "NDI", timeoutS, 640, 360);
            juce::Thread::sleep (1500);
            std::cout << "      broadcaster: " << bc.getStatus().ndi << std::endl;
        }
    }

    // ---- 4. live sources are listed for the deck menu
    {
        std::vector<live::SourceInfo> sp, nd;
        juce::String notes;
        live::listSources (sp, nd, notes);
        std::cout << "      live sources: " << sp.size() << " Spout, " << nd.size() << " NDI" << (notes.isNotEmpty() ? "  (" + notes + ")" : juce::String()) << std::endl;
    }

    bc.setFrameTap (nullptr);
    s.spout = s.ndi = false;
    bc.setSettings (s);
    report (! bc.getStatus().running, "broadcaster stops when both outputs are switched off");

    std::cout << (failures == 0 ? "ALL PASSED" : juce::String (failures) + " FAILURE(S)") << std::endl;
    return failures == 0 ? 0 : 1;
}
