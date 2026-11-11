#include "MediaDeck.h"
#include <juce_graphics/juce_graphics.h>

#include "FFmpegApi.h"



namespace moshdeck
{
static juce::File logFile()
{
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory);
    dir.createDirectory();
    return dir.getChildFile ("MoshDeckVJ.log");
}

static void logLine (const juce::String& s)
{
    static std::mutex m;
    std::lock_guard<std::mutex> l (m);
    logFile().appendText (juce::Time::getCurrentTime().toString (true, true, true, true) + "  " + s + "\n");
}

/** Returns an empty string when FFmpeg is usable, otherwise a user-facing reason. */
static juce::String ensureCodecLibraries()
{
#if MOSHDECK_HAVE_FFMPEG
    return loadFFmpeg ([] (const juce::String& m) { logLine (m); });
#else
    return {};
#endif
}

#if MOSHDECK_HAVE_FFMPEG
static juce::String avError (int code)
{
    char buf[256] = {};
    ffapi().av_strerror (code, buf, sizeof (buf));
    return juce::String (buf) + " (" + juce::String (code) + ")";
}
#endif

struct MediaDeck::Impl
{
#if MOSHDECK_HAVE_FFMPEG
    AVFormatContext* fmt = nullptr;
    AVCodecContext* codec = nullptr;
    SwsContext* sws = nullptr;
    AVFrame* av = nullptr;
    AVPacket* pkt = nullptr;
    int stream = -1;
    double timeBase = 0.0;
    double lastPts = -1.0;
    std::vector<uint8_t> rgb;
    int outW = 0, outH = 0;
    bool eof = false;
#endif
};

MediaDeck::MediaDeck() : impl (std::make_unique<Impl>())
{
    worker = std::thread ([this] { run(); });
}

MediaDeck::~MediaDeck()
{
    quit = true;
    cv.notify_all();
    if (worker.joinable()) worker.join();
    closeFile();
}

bool MediaDeck::hasVideoSupport()
{
#if MOSHDECK_HAVE_FFMPEG
    return true;
#else
    return false;
#endif
}

juce::String MediaDeck::supportedWildcard()
{
    juce::String w = "*.png;*.jpg;*.jpeg;*.gif;*.bmp";
    if (hasVideoSupport()) w << ";*.mp4;*.mov;*.mkv;*.avi;*.webm;*.m4v;*.wmv;*.mpg;*.mpeg;*.flv";
    return w;
}

void MediaDeck::load (const juce::String& path)
{
    {
        std::lock_guard<std::mutex> l (mutex);
        pendingPath = path;
        hasPending = true;
    }
    cv.notify_all();
}

juce::String MediaDeck::getPath() const
{
    std::lock_guard<std::mutex> l (mutex);
    return currentPath;
}

juce::String MediaDeck::getStatus() const
{
    std::lock_guard<std::mutex> l (mutex);
    return status;
}

void MediaDeck::tick (double dt, double speed, double beats, double loopBeats, double cue)
{
    const double dur = duration.load();
    if (dur <= 0.0) return;
    double p;
    if (loopBeats > 0.0)
    {
        const double phase = std::fmod (beats, loopBeats) / loopBeats;
        p = cue * dur + (speed >= 0 ? phase : 1.0 - phase) * dur;
    }
    else
        p = position.load() + dt * speed;
    p = std::fmod (p, dur);
    if (p < 0) p += dur;
    position = p;
    target = p;
    cv.notify_all();
}

bool MediaDeck::fetch (VideoFrame& dest, uint64_t lastSerial)
{
    std::lock_guard<std::mutex> l (frameMutex);
    if (frame.serial == lastSerial || frame.width == 0) return false;
    dest.width = frame.width;
    dest.height = frame.height;
    dest.serial = frame.serial;
    dest.rgba = frame.rgba;
    return true;
}

void MediaDeck::publish (const uint8_t* src, int w, int h, int stride)
{
    std::lock_guard<std::mutex> l (frameMutex);
    frame.width = w; frame.height = h;
    frame.rgba.resize ((size_t) (w * h * 4));
    for (int y = 0; y < h; ++y) // flip to bottom-up
        std::memcpy (&frame.rgba[(size_t) ((h - 1 - y) * w * 4)], src + (size_t) y * (size_t) stride, (size_t) w * 4);
    ++frame.serial;
}

//==============================================================================
void MediaDeck::run()
{
    while (! quit)
    {
        juce::String toOpen;
        bool open = false;
        {
            std::unique_lock<std::mutex> l (mutex);
            cv.wait_for (l, std::chrono::milliseconds (video ? 4 : 50), [this] { return quit.load() || hasPending; });
            if (quit) break;
            if (hasPending) { toOpen = pendingPath; hasPending = false; open = true; }
        }
        if (open)
        {
            const bool ok = openFile (toOpen);
            std::lock_guard<std::mutex> l (mutex);
            currentPath = toOpen;
            if (! ok && status.isEmpty()) status = "Could not open file";
            continue;
        }
        if (video)
            decodeUntil (target.load());
    }
}

void MediaDeck::closeFile()
{
#if MOSHDECK_HAVE_FFMPEG
    auto& m = *impl;
    if (m.sws) ffapi().sws_freeContext (m.sws);
    if (m.av) ffapi().av_frame_free (&m.av);
    if (m.pkt) ffapi().av_packet_free (&m.pkt);
    if (m.codec) ffapi().avcodec_free_context (&m.codec);
    if (m.fmt) ffapi().avformat_close_input (&m.fmt);
    m = Impl();
#endif
    video = false;
    duration = 0.0;
}

bool MediaDeck::openFile (const juce::String& path)
{
    closeFile();
    {
        std::lock_guard<std::mutex> l (mutex);
        status.clear();
    }
    const juce::File f (path);
    if (! f.existsAsFile()) { std::lock_guard<std::mutex> l (mutex); status = "File not found"; return false; }
    const auto ext = f.getFileExtension().toLowerCase();
    const bool still = ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || (ext == ".gif" && ! hasVideoSupport());

    if (still)
    {
        auto img = juce::ImageFileFormat::loadFrom (f);
        if (! img.isValid()) { std::lock_guard<std::mutex> l (mutex); status = "Unsupported image"; return false; }
        img = img.convertedToFormat (juce::Image::ARGB);
        const int w = img.getWidth(), h = img.getHeight();
        std::vector<uint8_t> rgba ((size_t) (w * h * 4));
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::readOnly);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                const auto c = bd.getPixelColour (x, y);
                auto* d = &rgba[(size_t) ((y * w + x) * 4)];
                d[0] = c.getRed(); d[1] = c.getGreen(); d[2] = c.getBlue(); d[3] = c.getAlpha();
            }
        publish (rgba.data(), w, h, w * 4);
        duration = 1.0;
        position = 0.0;
        std::lock_guard<std::mutex> l (mutex);
        status = "Image " + juce::String (w) + "x" + juce::String (h);
        return true;
    }

#if MOSHDECK_HAVE_FFMPEG
    auto fail = [this, &path] (const juce::String& why)
    {
        logLine ("open failed: " + path + " : " + why);
        closeFile();
        std::lock_guard<std::mutex> l (mutex);
        status = why;
        return false;
    };
    const auto libProblem = ensureCodecLibraries();
    if (libProblem.isNotEmpty()) return fail (libProblem);
    auto& m = *impl;
    if (const int r = ffapi().avformat_open_input (&m.fmt, path.toRawUTF8(), nullptr, nullptr); r < 0)
        return fail ("FFmpeg could not open file: " + avError (r));
    if (const int r = ffapi().avformat_find_stream_info (m.fmt, nullptr); r < 0)
        return fail ("Could not read stream info: " + avError (r));
    m.stream = ffapi().av_find_best_stream (m.fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (m.stream < 0) return fail ("No video stream in file");
    auto* st = m.fmt->streams[m.stream];
    const AVCodec* dec = ffapi().avcodec_find_decoder (st->codecpar->codec_id);
    if (dec == nullptr) return fail (juce::String ("No decoder for codec ") + ffapi().avcodec_get_name (st->codecpar->codec_id));
    m.codec = ffapi().avcodec_alloc_context3 (dec);
    ffapi().avcodec_parameters_to_context (m.codec, st->codecpar);
    m.codec->thread_count = 0; // auto
    if (const int r = ffapi().avcodec_open2 (m.codec, dec, nullptr); r < 0)
        return fail (juce::String ("Could not open decoder ") + dec->name + ": " + avError (r));
    m.av = ffapi().av_frame_alloc();
    m.pkt = ffapi().av_packet_alloc();
    m.timeBase = av_q2d (st->time_base);
    double dur = st->duration != AV_NOPTS_VALUE ? (double) st->duration * m.timeBase
                                                : (m.fmt->duration != AV_NOPTS_VALUE ? (double) m.fmt->duration / AV_TIME_BASE : 10.0);
    if (dur <= 0.0) dur = 10.0;
    duration = dur;

    // scale large videos down to 1280 wide for real-time use
    m.outW = m.codec->width; m.outH = m.codec->height;
    if (m.outW > 1280) { m.outH = (int) ((double) m.outH * 1280.0 / m.outW) & ~1; m.outW = 1280; }
    if (m.codec->width <= 0 || m.codec->height <= 0 || m.codec->pix_fmt == AV_PIX_FMT_NONE)
        return fail ("Video has no size / pixel format");
    m.sws = ffapi().sws_getContext (m.codec->width, m.codec->height, m.codec->pix_fmt, m.outW, m.outH, AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (m.sws == nullptr) return fail ("Could not create colour converter");
    m.rgb.resize ((size_t) (m.outW * m.outH * 4));
    m.lastPts = -1.0;
    position = 0.0;
    target = 0.0;
    video = true;
    {
        std::lock_guard<std::mutex> l (mutex);
        logLine ("opened " + path + " codec=" + dec->name);
        status = "Video " + juce::String (m.codec->width) + "x" + juce::String (m.codec->height) + ", " + juce::String (dur, 1) + "s";
    }
    decodeUntil (0.0);
    return true;
#else
    std::lock_guard<std::mutex> l (mutex);
    status = "Video support not compiled in (build with FFmpeg)";
    return false;
#endif
}

bool MediaDeck::decodeUntil (double t)
{
#if MOSHDECK_HAVE_FFMPEG
    auto& m = *impl;
    if (m.fmt == nullptr) return false;
    const double frameDur = 1.0 / 30.0;
    if (m.lastPts >= 0.0 && t >= m.lastPts && t < m.lastPts + frameDur * 0.5)
        return true; // current frame still valid

    if (m.lastPts < 0.0 || t < m.lastPts - 0.05 || t > m.lastPts + 1.5 || m.eof)
    {
        const auto ts = (int64_t) (t / m.timeBase);
        ffapi().av_seek_frame (m.fmt, m.stream, ts, AVSEEK_FLAG_BACKWARD);
        ffapi().avcodec_flush_buffers (m.codec);
        m.eof = false;
        m.lastPts = -1.0;
    }

    int guard = 600;
    while (guard-- > 0)
    {
        const int r = ffapi().av_read_frame (m.fmt, m.pkt);
        if (r < 0) { m.eof = true; ffapi().avcodec_send_packet (m.codec, nullptr); }
        else if (m.pkt->stream_index != m.stream) { ffapi().av_packet_unref (m.pkt); continue; }
        else { ffapi().avcodec_send_packet (m.codec, m.pkt); ffapi().av_packet_unref (m.pkt); }

        bool got = false;
        while (ffapi().avcodec_receive_frame (m.codec, m.av) == 0)
        {
            const int64_t pts = m.av->best_effort_timestamp != AV_NOPTS_VALUE ? m.av->best_effort_timestamp : m.av->pts;
            const double ft = pts != AV_NOPTS_VALUE ? (double) pts * m.timeBase : (m.lastPts < 0 ? 0.0 : m.lastPts + frameDur);
            m.lastPts = ft;
            if (ft + frameDur >= t)
            {
                uint8_t* dst[4] = { m.rgb.data(), nullptr, nullptr, nullptr };
                int lines[4] = { m.outW * 4, 0, 0, 0 };
                ffapi().sws_scale (m.sws, m.av->data, m.av->linesize, 0, m.codec->height, dst, lines);
                publish (m.rgb.data(), m.outW, m.outH, m.outW * 4);
                got = true;
            }
            ffapi().av_frame_unref (m.av);
            if (got) break;
        }
        if (got) return true;
        if (m.eof) return false;
    }
#else
    juce::ignoreUnused (t);
#endif
    return false;
}
} // namespace moshdeck
