#include "Broadcaster.h"
#include "HeadlessGL.h"
#include "NdiApi.h"
#include "SpoutIO.h"
#include "render/RenderEngine.h"

namespace moshdeck
{
using namespace juce::gl;

struct Broadcaster::Senders
{
    spout::Sender spout;
    ndi::Sender ndi;
    bool spoutActive = false, ndiActive = false;
};

static std::atomic<int> instanceCounter { 0 };

Broadcaster::Broadcaster (EngineHost& h)
    : juce::Thread ("MoshDeck Spout/NDI"), host (h), senders (std::make_unique<Senders>())
{
    const int n = ++instanceCounter;
    defaultName = n == 1 ? juce::String ("MoshDeck VJ") : "MoshDeck VJ " + juce::String (n);
    settings.spoutName = settings.ndiName = defaultName;
}

Broadcaster::~Broadcaster()
{
    stopThread (5000);
}

void Broadcaster::setSettings (const Settings& s)
{
    {
        const juce::ScopedLock sl (lock);
        settings = s;
        settings.fps = juce::jlimit (1, 240, s.fps);
        if (settings.spoutName.trim().isEmpty()) settings.spoutName = defaultName;
        if (settings.ndiName.trim().isEmpty()) settings.ndiName = defaultName;
    }
    updateThread();
}

Broadcaster::Settings Broadcaster::getSettings() const
{
    const juce::ScopedLock sl (lock);
    return settings;
}

Broadcaster::Status Broadcaster::getStatus() const
{
    const juce::ScopedLock sl (lock);
    return status;
}

void Broadcaster::setFrameTap (std::function<void (const uint8_t*, int, int)> tap)
{
    {
        const juce::ScopedLock sl (lock);
        frameTap = std::move (tap);
    }
    updateThread();
}

void Broadcaster::updateThread()
{
    bool wanted;
    {
        const juce::ScopedLock sl (lock);
        wanted = settings.spout || settings.ndi || frameTap != nullptr;
    }
    if (wanted)
    {
        if (! isThreadRunning()) startThread (juce::Thread::Priority::high);
    }
    else if (isThreadRunning())
    {
        signalThreadShouldExit();
        notify();
        stopThread (5000);
        const juce::ScopedLock sl (lock);
        status = Status();
        status.spout = status.ndi = "Off";
    }
}

//==============================================================================
void Broadcaster::deliver (const uint8_t* px, int w, int h, const Settings& s)
{
    auto& tx = *senders;
    juce::String spoutStatus, ndiStatus;

    if (s.spout) { tx.spout.send (s.spoutName, px, w, h, spoutStatus); tx.spoutActive = true; }
    else if (tx.spoutActive) { tx.spout.close(); tx.spoutActive = false; }

    if (s.ndi) { tx.ndi.send (s.ndiName, px, w, h, s.fps, ndiStatus); tx.ndiActive = true; }
    else if (tx.ndiActive) { tx.ndi.close(); tx.ndiActive = false; }

    std::function<void (const uint8_t*, int, int)> tap;
    {
        const juce::ScopedLock sl (lock);
        tap = frameTap;
    }
    if (tap) tap (px, w, h);

    const double now = juce::Time::getMillisecondCounterHiRes();
    const juce::ScopedLock sl (lock);
    if (lastDeliver > 0.0 && now > lastDeliver)
        status.fps = status.fps * 0.9 + (1000.0 / (now - lastDeliver)) * 0.1;
    lastDeliver = now;
    status.running = true;
    status.width = w; status.height = h;
    status.spout = s.spout ? spoutStatus : juce::String ("Off");
    status.ndi = s.ndi ? ndiStatus : juce::String ("Off");
}

void Broadcaster::run()
{
    HeadlessGL glc;
    juce::String err;
    if (! glc.create (err))
    {
        const juce::ScopedLock sl (lock);
        status = Status();
        status.render = "Spout/NDI output can't start: " + err;
        status.spout = status.ndi = "Off";
        return;
    }
    juce::gl::loadFunctions();

    {
        const juce::ScopedLock sl (lock);
        status.render = "Rendering";
    }

    auto engine = std::make_unique<RenderEngine> (host);
    engine->contextCreated();

    // 8-bit target the finished frame is blitted into (flipped, so rows come back top-first)
    GLuint fbo = 0, tex = 0;
    int fw = 0, fh = 0;
    constexpr int numPbos = 2;
    GLuint pbo[numPbos] = {};
    GLsync fence[numPbos] = {};
    int pboW[numPbos] = {}, pboH[numPbos] = {};
    size_t pboSize[numPbos] = {};
    int head = 0;
    glGenBuffers (numPbos, pbo);

    double next = juce::Time::getMillisecondCounterHiRes();
    lastDeliver = 0.0;

    while (! threadShouldExit())
    {
        glc.pumpMessages();
        const auto s = getSettings();
        const double period = 1000.0 / juce::jlimit (1, 240, s.fps);
        const double now = juce::Time::getMillisecondCounterHiRes();
        if (now < next)
        {
            wait (juce::jmax (1, (int) (next - now)));
            continue;
        }
        next += period;
        if (now - next > period * 3.0) next = now + period; // fell behind: don't try to catch up

        engine->renderFrame (16, 16, 1.0f, false);
        const auto& out = engine->getOutput();
        if (out.fbo == 0 || out.w <= 0 || out.h <= 0) continue;

        if (fbo == 0 || fw != out.w || fh != out.h)
        {
            if (fbo) glDeleteFramebuffers (1, &fbo);
            if (tex) glDeleteTextures (1, &tex);
            fw = out.w; fh = out.h;
            glGenTextures (1, &tex);
            glBindTexture (GL_TEXTURE_2D, tex);
            glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, fw, fh, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glGenFramebuffers (1, &fbo);
            glBindFramebuffer (GL_FRAMEBUFFER, fbo);
            glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
        }
        glBindFramebuffer (GL_READ_FRAMEBUFFER, out.fbo);
        glBindFramebuffer (GL_DRAW_FRAMEBUFFER, fbo);
        glBlitFramebuffer (0, 0, fw, fh, 0, fh, fw, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST); // vertical flip

        // start reading this frame into a PBO
        glBindFramebuffer (GL_READ_FRAMEBUFFER, fbo);
        glBindBuffer (GL_PIXEL_PACK_BUFFER, pbo[head]);
        const size_t bytes = (size_t) fw * (size_t) fh * 4;
        if (pboSize[head] != bytes)
        {
            glBufferData (GL_PIXEL_PACK_BUFFER, (GLsizeiptr) bytes, nullptr, GL_STREAM_READ);
            pboSize[head] = bytes;
        }
        glPixelStorei (GL_PACK_ALIGNMENT, 4);
        glReadPixels (0, 0, fw, fh, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
        if (fence[head]) glDeleteSync (fence[head]);
        fence[head] = glFenceSync (GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        pboW[head] = fw; pboH[head] = fh;

        // hand on the previous frame, which has had a whole frame to arrive
        const int prev = (head + 1) % numPbos;
        if (fence[prev] != nullptr)
        {
            const GLenum w = glClientWaitSync (fence[prev], GL_SYNC_FLUSH_COMMANDS_BIT, 100000000); // 100 ms
            glDeleteSync (fence[prev]);
            fence[prev] = nullptr;
            if (w == GL_ALREADY_SIGNALED || w == GL_CONDITION_SATISFIED)
            {
                glBindBuffer (GL_PIXEL_PACK_BUFFER, pbo[prev]);
                const size_t n = (size_t) pboW[prev] * (size_t) pboH[prev] * 4;
                if (const auto* px = static_cast<const uint8_t*> (glMapBufferRange (GL_PIXEL_PACK_BUFFER, 0, (GLsizeiptr) n, GL_MAP_READ_BIT)))
                {
                    deliver (px, pboW[prev], pboH[prev], s);
                    glUnmapBuffer (GL_PIXEL_PACK_BUFFER);
                }
            }
        }
        glBindBuffer (GL_PIXEL_PACK_BUFFER, 0);
        glBindFramebuffer (GL_FRAMEBUFFER, 0);
        head = prev;
    }

    senders->spout.close();
    senders->ndi.close();
    senders->spoutActive = senders->ndiActive = false;
    for (auto& f : fence) if (f) glDeleteSync (f);
    glDeleteBuffers (numPbos, pbo);
    if (fbo) glDeleteFramebuffers (1, &fbo);
    if (tex) glDeleteTextures (1, &tex);
    engine->contextClosing();
    engine.reset();
    glc.release();
}
} // namespace moshdeck
