#pragma once
#include <juce_core/juce_core.h>
#include <memory>

namespace moshdeck
{
/**
    An invisible OpenGL 3.3 core context that belongs to the thread that creates it.
    Used by the Spout/NDI broadcaster so it can render while no MoshDeck window is open.
    Windows: hidden window + WGL. Linux: GLX pbuffer (needs an X display). macOS: not supported.
*/
class HeadlessGL
{
public:
    HeadlessGL();
    ~HeadlessGL();
    /** Creates the context and makes it current on the calling thread. */
    bool create (juce::String& error);
    /** Windows: services the hidden window's message queue. Call regularly from the owning thread. */
    void pumpMessages();
    void release();
    bool isValid() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
} // namespace moshdeck
