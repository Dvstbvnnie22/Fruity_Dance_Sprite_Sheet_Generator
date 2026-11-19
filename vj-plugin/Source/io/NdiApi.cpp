#include "NdiApi.h"
#include <atomic>
#include <cstring>

namespace moshdeck::ndi
{
namespace
{
    std::mutex loadMutex;
    bool loadAttempted = false;
    Api table;
    bool tableOk = false;
    juce::String loadProblem, loadedFrom;

    juce::StringArray candidatePaths()
    {
        juce::StringArray paths;
       #if JUCE_WINDOWS
        const juce::String dll = "Processing.NDI.Lib.x64.dll";
        for (auto* var : { "NDI_RUNTIME_DIR_V6", "NDI_RUNTIME_DIR_V5" })
        {
            const auto dir = juce::SystemStats::getEnvironmentVariable (var, {});
            if (dir.isNotEmpty()) paths.add (juce::File (dir).getChildFile (dll).getFullPathName());
        }
        const auto pf = juce::File::getSpecialLocation (juce::File::globalApplicationsDirectory); // C:\Program Files
        for (auto* sub : { "NDI/NDI 6 Runtime/v6", "NDI/NDI 6 Tools/Runtime", "NDI/NDI 6 SDK/Bin/x64", "NDI/NDI 5 Runtime/v5" })
            paths.add (pf.getChildFile (sub).getChildFile (dll).getFullPathName());
        paths.add (juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory().getChildFile (dll).getFullPathName());
        paths.add (dll); // normal DLL search path
       #elif JUCE_MAC
        paths.add ("/Library/NDI SDK for Apple/lib/macOS/libndi.dylib");
        paths.add ("/usr/local/lib/libndi.dylib");
        paths.add ("libndi.dylib");
       #else
        const auto dir = juce::SystemStats::getEnvironmentVariable ("NDI_RUNTIME_DIR_V6", {});
        if (dir.isNotEmpty()) paths.add (juce::File (dir).getChildFile ("libndi.so.6").getFullPathName());
        paths.add ("libndi.so.6");
        paths.add ("libndi.so");
       #endif
        paths.removeDuplicates (true);
        return paths;
    }

    template <typename Fn>
    bool resolve (juce::DynamicLibrary& lib, Fn& fn, const char* name, juce::String& missing)
    {
        fn = reinterpret_cast<Fn> (lib.getFunction (name));
        if (fn == nullptr) missing << name << " ";
        return fn != nullptr;
    }
} // namespace

const Api* api (juce::String* problemOut)
{
    std::lock_guard<std::mutex> l (loadMutex);
    if (! loadAttempted)
    {
        loadAttempted = true;
        // Deliberately leaked: the runtime stays loaded for the life of the process.
        auto* lib = new juce::DynamicLibrary();
        bool opened = false;
        for (auto& p : candidatePaths())
        {
            const bool absolute = juce::File::isAbsolutePath (p);
            if (absolute && ! juce::File (p).existsAsFile()) continue;
            if (lib->open (p)) { opened = true; loadedFrom = p; break; }
        }
        if (! opened)
            loadProblem = "NDI runtime not found. Install NDI Tools (or the NDI Runtime) from ndi.video.";
        else
        {
            juce::String missing;
            bool ok = true;
            ok &= resolve (*lib, table.initialize, "NDIlib_initialize", missing);
            ok &= resolve (*lib, table.version, "NDIlib_version", missing);
            ok &= resolve (*lib, table.find_create_v2, "NDIlib_find_create_v2", missing);
            ok &= resolve (*lib, table.find_destroy, "NDIlib_find_destroy", missing);
            ok &= resolve (*lib, table.find_get_current_sources, "NDIlib_find_get_current_sources", missing);
            ok &= resolve (*lib, table.find_wait_for_sources, "NDIlib_find_wait_for_sources", missing);
            ok &= resolve (*lib, table.recv_create_v3, "NDIlib_recv_create_v3", missing);
            ok &= resolve (*lib, table.recv_destroy, "NDIlib_recv_destroy", missing);
            ok &= resolve (*lib, table.recv_capture_v3, "NDIlib_recv_capture_v3", missing);
            ok &= resolve (*lib, table.recv_free_video_v2, "NDIlib_recv_free_video_v2", missing);
            ok &= resolve (*lib, table.send_create, "NDIlib_send_create", missing);
            ok &= resolve (*lib, table.send_destroy, "NDIlib_send_destroy", missing);
            ok &= resolve (*lib, table.send_send_video_async_v2, "NDIlib_send_send_video_async_v2", missing);
            ok &= resolve (*lib, table.send_get_no_connections, "NDIlib_send_get_no_connections", missing);
            if (! ok)
                loadProblem = "NDI runtime at " + loadedFrom + " is too old (missing " + missing.trim() + "). Update NDI Tools.";
            else if (! table.initialize())
                loadProblem = "NDI could not start on this computer (unsupported CPU).";
            else
                tableOk = true;
        }
    }
    if (problemOut != nullptr) *problemOut = loadProblem;
    return tableOk ? &table : nullptr;
}

juce::String runtimePath()
{
    std::lock_guard<std::mutex> l (loadMutex);
    return tableOk ? loadedFrom : juce::String();
}

//==============================================================================
struct Finder::Impl : public juce::Thread
{
    Impl() : juce::Thread ("MoshDeck NDI finder") {}
    ~Impl() override { stopThread (3000); }

    void run() override
    {
        const auto* a = api();
        if (a == nullptr) return;
        FindCreate fc;
        fc.showLocalSources = true;
        void* finder = a->find_create_v2 (&fc);
        if (finder == nullptr) return;
        while (! threadShouldExit())
        {
            a->find_wait_for_sources (finder, 250);
            uint32_t n = 0;
            const Source* src = a->find_get_current_sources (finder, &n);
            juce::StringArray names;
            for (uint32_t i = 0; i < n; ++i)
                if (src[i].name != nullptr) names.add (juce::String::fromUTF8 (src[i].name));
            {
                const juce::ScopedLock sl (lock);
                sources = names;
            }
        }
        a->find_destroy (finder);
    }

    juce::CriticalSection lock;
    juce::StringArray sources;
};

Finder& Finder::get()
{
    // Intentionally never destroyed during static teardown (a DLL must not join threads then);
    // its thread is stopped when the last FinderLease goes away.
    static Finder* f = new Finder();
    return *f;
}

void Finder::start()
{
    std::lock_guard<std::mutex> l (mutex);
    if (impl == nullptr)
    {
        if (api() == nullptr) return;
        impl = std::make_unique<Impl>();
        impl->startThread();
    }
}

juce::StringArray Finder::getSources()
{
    start();
    std::lock_guard<std::mutex> l (mutex);
    if (impl == nullptr) return {};
    const juce::ScopedLock sl (impl->lock);
    return impl->sources;
}

void Finder::stop()
{
    std::unique_ptr<Impl> old;
    {
        std::lock_guard<std::mutex> l (mutex);
        old = std::move (impl);
    }
    old.reset(); // joins the thread
}

Finder::~Finder() { stop(); }

static std::atomic<int> finderLeases { 0 };
FinderLease::FinderLease() { ++finderLeases; }
FinderLease::~FinderLease() { if (--finderLeases == 0) Finder::get().stop(); }

//==============================================================================
Receiver::~Receiver() { close(); }

void Receiver::close()
{
    if (inst != nullptr)
        if (const auto* a = api())
            a->recv_destroy (inst);
    inst = nullptr;
}

bool Receiver::open (const juce::String& sourceName, juce::String& status)
{
    close();
    name = sourceName;
    frames = 0;
    juce::String problem;
    const auto* a = api (&problem);
    if (a == nullptr) { status = problem; return false; }
    RecvCreateV3 rc;
    const auto utf8 = sourceName.toStdString();
    rc.source.name = utf8.c_str();
    rc.colorFormat = 2;   // RGBX_RGBA
    rc.bandwidth = 100;   // highest
    rc.allowVideoFields = false;
    rc.recvName = "MoshDeck VJ";
    inst = a->recv_create_v3 (&rc);
    if (inst == nullptr) { status = "NDI: could not create a receiver for " + sourceName; return false; }
    status = "NDI: connecting to " + sourceName + "...";
    return true;
}

int Receiver::receive (std::vector<uint8_t>& rgba, int& w, int& h, uint32_t timeoutMs, juce::String& status)
{
    const auto* a = api();
    if (a == nullptr || inst == nullptr) return -1;
    VideoFrameV2 v;
    const int type = a->recv_capture_v3 (inst, &v, nullptr, nullptr, timeoutMs);
    if (type == frameTypeError) { status = "NDI: lost connection to " + name; return -1; }
    if (type != frameTypeVideo) return 0;

    int result = 0;
    if (v.data != nullptr && v.xres > 0 && v.yres > 0
        && (v.fourCC == fourCC_RGBA || v.fourCC == fourCC_RGBX || v.fourCC == fourCC_BGRA || v.fourCC == fourCC_BGRX))
    {
        w = v.xres; h = v.yres;
        const int stride = v.lineStrideBytes > 0 ? v.lineStrideBytes : w * 4;
        rgba.resize ((size_t) w * (size_t) h * 4);
        const bool swap = v.fourCC == fourCC_BGRA || v.fourCC == fourCC_BGRX;
        const bool opaque = v.fourCC == fourCC_RGBX || v.fourCC == fourCC_BGRX;
        for (int y = 0; y < h; ++y)
        {
            const uint8_t* s = v.data + (size_t) y * (size_t) stride;
            uint8_t* d = rgba.data() + (size_t) y * (size_t) w * 4;
            if (! swap && ! opaque) { std::memcpy (d, s, (size_t) w * 4); continue; }
            for (int x = 0; x < w; ++x, s += 4, d += 4)
            {
                d[0] = swap ? s[2] : s[0]; d[1] = s[1]; d[2] = swap ? s[0] : s[2];
                d[3] = opaque ? (uint8_t) 255 : s[3];
            }
        }
        ++frames;
        status = "NDI: " + name + "  " + juce::String (w) + "x" + juce::String (h);
        result = 1;
    }
    else if (v.data != nullptr)
        status = "NDI: unsupported pixel format from " + name;
    a->recv_free_video_v2 (inst, &v);
    return result;
}

//==============================================================================
Sender::~Sender() { close(); }

void Sender::close()
{
    if (inst != nullptr)
        if (const auto* a = api())
        {
            a->send_send_video_async_v2 (inst, nullptr); // wait until the last async frame is done with our buffer
            a->send_destroy (inst);
        }
    inst = nullptr;
    name.clear();
    lastConnections = 0;
}

bool Sender::send (const juce::String& sourceName, const uint8_t* bgra, int w, int h, int fps, juce::String& status)
{
    juce::String problem;
    const auto* a = api (&problem);
    if (a == nullptr) { status = problem; return false; }
    if (inst == nullptr || sourceName != name)
    {
        close();
        SendCreate sc;
        const auto utf8 = sourceName.toStdString();
        sc.name = utf8.c_str();
        sc.clockVideo = false; // MoshDeck paces its own frames
        sc.clockAudio = false;
        inst = a->send_create (&sc);
        if (inst == nullptr) { status = "NDI: could not create sender '" + sourceName + "'"; return false; }
        name = sourceName;
        sent = 0;
    }
    auto& buf = buffers[cur];
    buf.resize ((size_t) w * (size_t) h * 4);
    std::memcpy (buf.data(), bgra, buf.size());
    VideoFrameV2 v;
    v.xres = w; v.yres = h;
    v.fourCC = fourCC_BGRX;
    v.frameRateN = juce::jmax (1, fps) * 1000; v.frameRateD = 1000;
    v.pictureAspectRatio = (float) w / (float) juce::jmax (1, h);
    v.frameFormatType = 1;
    v.data = buf.data();
    v.lineStrideBytes = w * 4;
    a->send_send_video_async_v2 (inst, &v); // the previous buffer is now free again
    cur = 1 - cur;
    if ((sent++ % 30) == 0) lastConnections = a->send_get_no_connections (inst, 0);
    status = "NDI: sending '" + name + "' " + juce::String (w) + "x" + juce::String (h) + ", "
           + juce::String (lastConnections) + (lastConnections == 1 ? " receiver" : " receivers");
    return true;
}
} // namespace moshdeck::ndi
