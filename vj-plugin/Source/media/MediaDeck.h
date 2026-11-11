#pragma once
#include <juce_core/juce_core.h>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace moshdeck
{
/** RGBA8 frame, rows bottom-to-top (ready for glTexImage2D). */
struct VideoFrame
{
    std::vector<uint8_t> rgba;
    int width = 0, height = 0;
    uint64_t serial = 0;
};

/**
    One media "main track": plays a still image, animated GIF or video file
    (video via FFmpeg when available). Decoding runs on a background thread;
    the render thread drives the playhead (free-running or locked to the beat)
    and pulls the newest frame.
*/
class MediaDeck
{
public:
    MediaDeck();
    ~MediaDeck();

    void load (const juce::String& path);          // async
    juce::String getPath() const;
    bool isVideo() const { return video.load(); }
    double getDuration() const { return duration.load(); }
    double getPosition() const { return position.load(); }
    juce::String getStatus() const;

    /** Advance the playhead. loopBeats > 0 stretches the clip so it loops exactly every N beats. */
    void tick (double dtSeconds, double speed, double beats, double loopBeats, double cue);

    /** Copies the newest frame if its serial differs from `lastSerial`. */
    bool fetch (VideoFrame& dest, uint64_t lastSerial);

    static juce::String supportedWildcard();
    static bool hasVideoSupport();

private:
    void run();
    bool openFile (const juce::String& path);
    void closeFile();
    bool decodeUntil (double t);
    void publish (const uint8_t* rgbaTopDown, int w, int h, int strideBytes);

    struct Impl;
    std::unique_ptr<Impl> impl;

    std::thread worker;
    mutable std::mutex mutex;
    std::condition_variable cv;
    std::atomic<bool> quit { false };
    juce::String pendingPath, currentPath, status;
    bool hasPending = false;

    std::mutex frameMutex;
    VideoFrame frame;

    std::atomic<double> position { 0.0 }, target { 0.0 }, duration { 0.0 };
    std::atomic<bool> video { false };
};
} // namespace moshdeck
