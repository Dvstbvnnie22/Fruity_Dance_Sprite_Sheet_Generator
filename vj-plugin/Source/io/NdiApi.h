#pragma once
#include <juce_core/juce_core.h>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

/*
    NDI(R) support.

    MoshDeck does not link or ship any part of the NDI SDK. The NDI runtime
    (Processing.NDI.Lib.x64.dll on Windows, libndi.so.6 on Linux) is installed
    on the user's machine by NDI Tools or the NDI Runtime installer, and is
    loaded at runtime. Only the handful of C functions below are used, so the
    plugin builds anywhere (including CI) without the SDK.

    The structs mirror the C ABI of the NDI 6 SDK (Processing.NDI.structs.h,
    Find.h, Recv.h, Send.h). Their sizes are checked with static_asserts.

    NDI(R) is a registered trademark of Vizrt NDI AB.
*/
namespace moshdeck::ndi
{
struct Source
{
    const char* name = nullptr;
    const char* url = nullptr;
};

struct FindCreate
{
    bool showLocalSources = true;
    const char* groups = nullptr;
    const char* extraIps = nullptr;
};

struct RecvCreateV3
{
    Source source;
    int32_t colorFormat = 2;   // NDIlib_recv_color_format_RGBX_RGBA
    int32_t bandwidth = 100;   // NDIlib_recv_bandwidth_highest
    bool allowVideoFields = false;
    const char* recvName = nullptr;
};

struct VideoFrameV2
{
    int32_t xres = 0, yres = 0;
    uint32_t fourCC = 0;
    int32_t frameRateN = 60000, frameRateD = 1000;
    float pictureAspectRatio = 0.0f;
    int32_t frameFormatType = 1; // progressive
    int64_t timecode = INT64_MAX; // NDIlib_send_timecode_synthesize
    uint8_t* data = nullptr;
    int32_t lineStrideBytes = 0;
    const char* metadata = nullptr;
    int64_t timestamp = 0;
};

struct SendCreate
{
    const char* name = nullptr;
    const char* groups = nullptr;
    bool clockVideo = false, clockAudio = false;
};

static_assert (sizeof (void*) != 8 || sizeof (Source) == 16, "NDI ABI: source");
static_assert (sizeof (void*) != 8 || sizeof (FindCreate) == 24, "NDI ABI: find_create");
static_assert (sizeof (void*) != 8 || sizeof (RecvCreateV3) == 40, "NDI ABI: recv_create_v3");
static_assert (sizeof (void*) != 8 || sizeof (VideoFrameV2) == 72, "NDI ABI: video_frame_v2");
static_assert (sizeof (void*) != 8 || sizeof (SendCreate) == 24, "NDI ABI: send_create");

constexpr uint32_t fourCC (char a, char b, char c, char d)
{
    return (uint32_t) (uint8_t) a | ((uint32_t) (uint8_t) b << 8) | ((uint32_t) (uint8_t) c << 16) | ((uint32_t) (uint8_t) d << 24);
}
constexpr uint32_t fourCC_RGBA = fourCC ('R', 'G', 'B', 'A');
constexpr uint32_t fourCC_RGBX = fourCC ('R', 'G', 'B', 'X');
constexpr uint32_t fourCC_BGRA = fourCC ('B', 'G', 'R', 'A');
constexpr uint32_t fourCC_BGRX = fourCC ('B', 'G', 'R', 'X');

constexpr int frameTypeNone = 0, frameTypeVideo = 1, frameTypeError = 4;

/** Function table resolved from the NDI runtime. */
struct Api
{
    bool (*initialize)() = nullptr;
    const char* (*version)() = nullptr;
    void* (*find_create_v2) (const FindCreate*) = nullptr;
    void (*find_destroy) (void*) = nullptr;
    const Source* (*find_get_current_sources) (void*, uint32_t*) = nullptr;
    bool (*find_wait_for_sources) (void*, uint32_t) = nullptr;
    void* (*recv_create_v3) (const RecvCreateV3*) = nullptr;
    void (*recv_destroy) (void*) = nullptr;
    int32_t (*recv_capture_v3) (void*, VideoFrameV2*, void* audio, void* metadata, uint32_t timeoutMs) = nullptr;
    void (*recv_free_video_v2) (void*, const VideoFrameV2*) = nullptr;
    void* (*send_create) (const SendCreate*) = nullptr;
    void (*send_destroy) (void*) = nullptr;
    void (*send_send_video_async_v2) (void*, const VideoFrameV2*) = nullptr;
    int32_t (*send_get_no_connections) (void*, uint32_t timeoutMs) = nullptr;
};

/** Loads and initialises the NDI runtime once. Returns nullptr (and the reason) when unavailable. */
const Api* api (juce::String* problemOut = nullptr);

/** Where the runtime was loaded from (empty until loaded). */
juce::String runtimePath();

/** Background source discovery, shared by the whole process. Hold a FinderLease while you need it. */
class Finder
{
public:
    static Finder& get();
    /** Full NDI names, e.g. "STUDIO-PC (OBS)". Starts discovery on first call. */
    juce::StringArray getSources();
    void stop();
    ~Finder();
private:
    Finder() = default;
    void start();
    struct Impl;
    std::unique_ptr<Impl> impl;
    std::mutex mutex;
};

/** Keeps NDI discovery allowed while alive; the discovery thread stops when the last lease is released. */
struct FinderLease
{
    FinderLease();
    ~FinderLease();
    FinderLease (const FinderLease&) = delete;
    FinderLease& operator= (const FinderLease&) = delete;
};

/** Receives one NDI source as RGBA frames (rows top-to-bottom). Not thread-safe; use from one thread. */
class Receiver
{
public:
    Receiver() = default;
    ~Receiver();
    bool open (const juce::String& sourceName, juce::String& status);
    /** Waits up to timeoutMs. Returns 1 for a new frame in `rgba` (w x h, tightly packed), 0 for none, -1 on error. */
    int receive (std::vector<uint8_t>& rgba, int& w, int& h, uint32_t timeoutMs, juce::String& status);
    void close();
private:
    void* inst = nullptr;
    juce::String name;
    int64_t frames = 0;
};

/** Sends BGRA frames (rows top-to-bottom) as an NDI source. Not thread-safe; use from one thread. */
class Sender
{
public:
    Sender() = default;
    ~Sender();
    /** (Re)creates the sender when the name changes. */
    bool send (const juce::String& sourceName, const uint8_t* bgra, int w, int h, int fps, juce::String& status);
    int connections() const { return lastConnections; }
    void close();
private:
    void* inst = nullptr;
    juce::String name;
    std::vector<uint8_t> buffers[2];
    int cur = 0;
    int lastConnections = 0;
    int64_t sent = 0;
};
} // namespace moshdeck::ndi
