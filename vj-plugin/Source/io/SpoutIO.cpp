#include "SpoutIO.h"

#if MOSHDECK_HAVE_SPOUT
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <SpoutDX.h>
 #include <string>
#endif

namespace moshdeck::spout
{
#if MOSHDECK_HAVE_SPOUT

bool isAvailable() { return true; }

juce::StringArray listSenders()
{
    juce::StringArray out;
    spoutDX dx; // only reads the shared sender list, no DirectX device is opened
    const int n = dx.GetSenderCount();
    for (int i = 0; i < n; ++i)
    {
        char name[256] = {};
        if (dx.GetSender (i, name, (int) sizeof (name)) && name[0] != 0)
            out.add (juce::String::fromUTF8 (name));
    }
    return out;
}

//==============================================================================
struct Receiver::Impl
{
    spoutDX dx;
    bool dxOpen = false;
    std::string name;
    std::vector<uint8_t> buf;
    unsigned int w = 0, h = 0;
    DXGI_FORMAT format = DXGI_FORMAT_B8G8R8A8_UNORM;
    double lastFrame = 0.0;
};

Receiver::Receiver() : impl (std::make_unique<Impl>()) {}
Receiver::~Receiver() { close(); }

void Receiver::close()
{
    if (impl->dxOpen)
    {
        impl->dx.ReleaseReceiver();
        impl->dx.CloseDirectX11();
        impl->dxOpen = false;
    }
    impl->buf.clear();
    impl->w = impl->h = 0;
}

bool Receiver::open (const juce::String& senderName, juce::String& status)
{
    close();
    impl->name = senderName.toStdString();
    if (! impl->dx.OpenDirectX11())
    {
        status = "Spout: DirectX 11 is not available";
        return false;
    }
    impl->dxOpen = true;
    impl->dx.SetReceiverName (impl->name.c_str());
    status = "Spout: waiting for '" + senderName + "'";
    return true;
}

int Receiver::receive (std::vector<uint8_t>& pixels, int& w, int& h, bool& bgra, juce::String& status)
{
    auto& m = *impl;
    if (! m.dxOpen) return 0;
    const juce::String nm (juce::String::fromUTF8 (m.name.c_str()));

    if (! m.dx.ReceiveImage (m.buf.empty() ? nullptr : m.buf.data(), m.w, m.h, false, false))
    {
        if (! m.dx.IsConnected()) status = "Spout: waiting for '" + nm + "'";
        return 0;
    }
    if (m.dx.IsUpdated())
    {
        // first connection, or the sender changed size/format: resize our buffer
        m.w = m.dx.GetSenderWidth();
        m.h = m.dx.GetSenderHeight();
        m.format = m.dx.GetSenderFormat();
        m.buf.assign ((size_t) m.w * m.h * 4, 0);
        return 0;
    }
    if (m.buf.empty() || m.w == 0 || m.h == 0) return 0;

    // only hand on genuinely new frames (or ~60 Hz when the sender doesn't count frames)
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (m.dx.IsFrameCountEnabled() ? ! m.dx.IsFrameNew() : (now - m.lastFrame < 15.0))
        return 0;

    switch (m.format)
    {
        case DXGI_FORMAT_B8G8R8A8_UNORM: case DXGI_FORMAT_B8G8R8X8_UNORM: case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        case DXGI_FORMAT_UNKNOWN: // DX9 senders
            bgra = true; break;
        case DXGI_FORMAT_R8G8B8A8_UNORM: case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
            bgra = false; break;
        default:
            status = "Spout: '" + nm + "' uses an unsupported texture format (" + juce::String ((int) m.format) + ")";
            return 0;
    }
    m.lastFrame = now;
    pixels = m.buf;
    w = (int) m.w; h = (int) m.h;
    status = "Spout: " + nm + "  " + juce::String (w) + "x" + juce::String (h);
    return 1;
}

//==============================================================================
struct Sender::Impl
{
    spoutDX dx;
    bool dxOpen = false;
    std::string name;
};

Sender::Sender() : impl (std::make_unique<Impl>()) {}
Sender::~Sender() { close(); }

void Sender::close()
{
    if (impl->dxOpen)
    {
        impl->dx.ReleaseSender();
        impl->dx.CloseDirectX11();
        impl->dxOpen = false;
    }
    impl->name.clear();
}

bool Sender::send (const juce::String& senderName, const uint8_t* bgra, int w, int h, juce::String& status)
{
    auto& m = *impl;
    if (! m.dxOpen)
    {
        if (! m.dx.OpenDirectX11()) { status = "Spout: DirectX 11 is not available"; return false; }
        m.dxOpen = true;
    }
    const auto wanted = senderName.toStdString();
    if (wanted != m.name)
    {
        m.dx.ReleaseSender();
        m.dx.SetSenderName (wanted.c_str());
        m.name = wanted;
    }
    if (! m.dx.SendImage (bgra, (unsigned int) w, (unsigned int) h))
    {
        status = "Spout: could not send '" + senderName + "'";
        return false;
    }
    // Spout may rename the sender if the name is already taken
    status = "Spout: sending '" + juce::String::fromUTF8 (m.dx.GetName()) + "' " + juce::String (w) + "x" + juce::String (h);
    return true;
}

#else // ------------------------------------------------------------- not Windows

bool isAvailable() { return false; }
juce::StringArray listSenders() { return {}; }

struct Receiver::Impl {};
Receiver::Receiver() : impl (std::make_unique<Impl>()) {}
Receiver::~Receiver() = default;
void Receiver::close() {}
bool Receiver::open (const juce::String&, juce::String& status) { status = "Spout is only available on Windows"; return false; }
int Receiver::receive (std::vector<uint8_t>&, int&, int&, bool&, juce::String&) { return 0; }

struct Sender::Impl {};
Sender::Sender() : impl (std::make_unique<Impl>()) {}
Sender::~Sender() = default;
void Sender::close() {}
bool Sender::send (const juce::String&, const uint8_t*, int, int, juce::String& status) { status = "Spout is only available on Windows"; return false; }

#endif
} // namespace moshdeck::spout
