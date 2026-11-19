#pragma once
#include <juce_core/juce_core.h>
#include <functional>

namespace moshdeck
{
struct EngineHost;

/**
    Sends MoshDeck's final mix over Spout (Windows) and NDI.

    It runs its own render engine in a hidden OpenGL context on a background
    thread, so the outputs keep going when the plugin editor and the output
    window are closed. Each frame is rendered at the canvas size, read back
    asynchronously (pixel buffer objects, one frame of latency) and handed to
    the enabled senders as BGRA, top row first.
*/
class Broadcaster : private juce::Thread
{
public:
    struct Settings
    {
        bool spout = false, ndi = false;
        juce::String spoutName, ndiName;
        int fps = 60;
    };

    struct Status
    {
        bool running = false;
        juce::String render, spout, ndi;
        double fps = 0.0;
        int width = 0, height = 0;
    };

    explicit Broadcaster (EngineHost&);
    ~Broadcaster() override;

    void setSettings (const Settings&);
    Settings getSettings() const;
    Status getStatus() const;

    /** "MoshDeck VJ" for the first plugin instance in the process, "MoshDeck VJ 2" for the second... */
    juce::String getDefaultName() const { return defaultName; }

    /** Test hook: called on the broadcast thread with every finished frame (BGRA, top row first).
        While a tap is set the renderer runs even with both outputs switched off. */
    void setFrameTap (std::function<void (const uint8_t* bgra, int w, int h)> tap);

private:
    void run() override;
    void updateThread();
    void deliver (const uint8_t* bgra, int w, int h, const Settings&);

    EngineHost& host;
    juce::String defaultName;
    mutable juce::CriticalSection lock;
    Settings settings;
    Status status;
    std::function<void (const uint8_t*, int, int)> frameTap;
    struct Senders;
    std::unique_ptr<Senders> senders; // owned and used by the broadcast thread only
    double lastDeliver = 0.0;
};
} // namespace moshdeck
