// Decodes a media file through MediaDeck exactly like the plugin does and reports
// status, duration, frame size and how many distinct frames arrive while playing.
// Usage: MoshDeckMediaTest <file> [seconds]
#include <juce_graphics/juce_graphics.h>
#include "media/MediaDeck.h"

int main (int argc, char** argv)
{
    if (argc < 2) { std::cerr << "usage: MoshDeckMediaTest <file> [seconds]\n"; return 2; }
    juce::ScopedJuceInitialiser_GUI init;
    moshdeck::MediaDeck deck;
    std::cout << "video support compiled in: " << (moshdeck::MediaDeck::hasVideoSupport() ? "yes" : "no") << "\n";
    std::cout << "file chooser pattern: " << moshdeck::MediaDeck::supportedWildcard() << "\n";
    deck.load (juce::String::fromUTF8 (argv[1]));
    const double seconds = argc > 2 ? std::atof (argv[2]) : 3.0;
    moshdeck::VideoFrame f;
    uint64_t serial = 0;
    int frames = 0;
    const auto start = juce::Time::getMillisecondCounterHiRes();
    double last = start;
    while (juce::Time::getMillisecondCounterHiRes() - start < seconds * 1000.0)
    {
        juce::Thread::sleep (16);
        const double now = juce::Time::getMillisecondCounterHiRes();
        deck.tick ((now - last) * 0.001, 1.0, 0.0, 0.0, 0.0);
        last = now;
        if (deck.fetch (f, serial)) { serial = f.serial; ++frames; }
    }
    std::cout << "status: " << deck.getStatus() << "\n"
              << "isVideo: " << deck.isVideo() << "  duration: " << deck.getDuration()
              << "  frame: " << f.width << "x" << f.height << "  distinct frames: " << frames << "\n";
    const bool ok = f.width > 0 && (! deck.isVideo() || frames > 10);
    std::cout << (ok ? "MEDIA TEST PASSED" : "MEDIA TEST FAILED") << "\n";
    return ok ? 0 : 1;
}
