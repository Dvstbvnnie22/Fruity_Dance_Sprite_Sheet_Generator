#pragma once
#include <juce_core/juce_core.h>
#include <memory>
#include <vector>

/*
    Spout (spout.zeal.co) texture sharing on Windows, through the SpoutDX
    (DirectX 11) part of the Spout2 SDK. Frames are exchanged as CPU pixel
    buffers, so nothing here depends on the OpenGL context of the caller and
    no GL/DX interop is needed. On other platforms everything reports
    "not available".
*/
namespace moshdeck::spout
{
bool isAvailable();

/** Names of the Spout senders currently running on this computer. */
juce::StringArray listSenders();

/** Receives one Spout sender. Use from a single thread. */
class Receiver
{
public:
    Receiver();
    ~Receiver();
    bool open (const juce::String& senderName, juce::String& status);
    /** Returns 1 when `pixels` holds a new w x h frame (rows top-to-bottom, BGRA if `bgra`, else RGBA), 0 otherwise. */
    int receive (std::vector<uint8_t>& pixels, int& w, int& h, bool& bgra, juce::String& status);
    void close();
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

/** Publishes BGRA frames (rows top-to-bottom) as a Spout sender. Use from a single thread. */
class Sender
{
public:
    Sender();
    ~Sender();
    bool send (const juce::String& senderName, const uint8_t* bgra, int w, int h, juce::String& status);
    void close();
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
} // namespace moshdeck::spout
